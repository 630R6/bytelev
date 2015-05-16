/*
                  ASSESSING THE SIMILARITY OF FILES

    USING (BOUNDS ON) THE LEVENSHTEIN DISTANCE BETWEEN BYTESTRINGS
*/



#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#if CHAR_BIT != 8
#  error unsupported platform
#endif



/*  Safe arithmetic operations for size_t

    Each of the following "size_t_* functions"
    succeeds if and only if it returns 0.
    Only some of the functions are currently used.
*/

int size_t_add(size_t * const res,
               size_t const op1,
               size_t const op2) {
  if (SIZE_MAX - op1 < op2) {
    return 1;
  }
  *res = op1 + op2;
  return 0;
}

int size_t_sub(size_t * const res,
               size_t const op1,
               size_t const op2) {
  if (op1 < op2) {
    return 1;
  }
  *res = op1 - op2;
  return 0;
}

int size_t_mul(size_t * const res,
               size_t const op1,
               size_t const op2) {
  if (op1 != 0) {
    if (SIZE_MAX / op1 < op2) {
      return 1;
    }
  }
  *res = op1 * op2;
  return 0;
}

int size_t_div(size_t * const res,
               size_t const op1,
               size_t const op2) {
  if (op2 == 0) {
    return 1;
  }
  *res = op1 / op2;
  return 0;
}

int size_t_mod(size_t * const res,
               size_t const op1,
               size_t const op2) {
  if (op2 == 0) {
    return 1;
  }
  *res = op1 % op2;
  return 0;
}

int size_t_from_string(size_t * const res, char const * const string) {
  char const * string_ = string;
  int all_zero = 1;
  char * end = NULL;
  long int long_int = 0;

  if ( *string_ == '\0' ||
       *string_ == '+'  ||
       *string_ == '-' ) {
    return 1;
  }
  for (; *string_ != '\0'; ++string_ ) {
    if ( *string_ != '0' ) {
      all_zero = 0;
    }
  }
  if (all_zero) {
    *res = 0;
    return 0;
  }

  long_int = strtol(string, &end, 10);
  if (long_int == 0 ||
      long_int == LONG_MAX ||
      long_int == LONG_MIN) {
    return 1;
  }
  if (!end) {
    return 1;
  }
  if (*end != '\0') {
    return 1;
  }

  if (long_int < 0 ||
      long_int > SIZE_MAX) {
    return 1;
  }

  *res = long_int;
  return 0;
}

int size_t_add_aug(size_t * const res, size_t const op) { return size_t_add(res, *res, op); }
int size_t_sub_aug(size_t * const res, size_t const op) { return size_t_sub(res, *res, op); }
int size_t_mul_aug(size_t * const res, size_t const op) { return size_t_mul(res, *res, op); }
int size_t_div_aug(size_t * const res, size_t const op) { return size_t_div(res, *res, op); }
int size_t_mod_aug(size_t * const res, size_t const op) { return size_t_mod(res, *res, op); }

int size_t_inc(size_t * const res) { return size_t_add_aug(res, 1); }
int size_t_dec(size_t * const res) { return size_t_sub_aug(res, 1); }



/*  Getting the size of a file

    The following function appears to be either
      - inefficient (ifdef SAFE_GET_FILE_SIZE) or
      - possibly unsafe.
    ("A binary stream need not meaningfully support fseek calls with a whence
    value of SEEK_END.")
    Future releases may improve the following function.
*/

int get_file_size(char const * const file_path, size_t * const file_size) {
  size_t file_size_ = 0;
  int ret = 0;
  FILE * file = NULL;

  file = fopen(file_path, "rb");
  if (!file) {
    return 1;
  }

#ifdef SAFE_GET_FILE_SIZE
  while ( EOF != fgetc(file) ) {
    ret = size_t_inc(&file_size_);
    if (ret) {
      fclose(file);
      return 1;
    }
  }
  ret = feof(file);
  if (!ret) {
    fclose(file);
    return 1;
  }
#else
  {
    long int long_int = 0;

    ret = fseek(file, 0, SEEK_END);
    if (ret) {
      fclose(file);
      return 1;
    }

    long_int = ftell(file);
    if (long_int < 0 ||
        long_int > SIZE_MAX)
    {
      fclose(file);
      return 1;
    }
    file_size_ = long_int;
  }
#endif

  fclose(file);
  *file_size = file_size_;
  return 0;
}



/*  struct buffer

    A buffer represents, in memory, the content of a file.
*/

typedef struct {
  char * pointer;
  size_t size;
} buffer;

void buffer_destroy(buffer * const buffer_) {
  if (buffer_) {
    free(buffer_->pointer);
  }
  free(buffer_);
}

int buffer_create(char const * const file_path,
                  size_t const max_size,
                  buffer ** const buffer_) {
  buffer * buf = NULL;
  int ret = 0;
  FILE * file = NULL;
  size_t fread_ = 0;

  buf = calloc( 1, sizeof(*buf) );
  if (!buf) {
    return 1;
  }
  buf->pointer = NULL;
  buf->size = 0;

  ret = get_file_size(file_path, &buf->size);
  if (ret) {
    buffer_destroy(buf);
    return ret;
  }
  if (buf->size > max_size) {
      buf->size = max_size;
  }

  if (buf->size) {
    buf->pointer = calloc(1, buf->size);
    if (!buf->pointer) {
      buffer_destroy(buf);
      return 1;
    }
  }

  file = fopen(file_path, "rb");
  if (!file) {
    buffer_destroy(buf);
    return 1;
  }
  fread_ = fread(buf->pointer, 1, buf->size, file);
  fclose(file);
  if (fread_ != buf->size) {
    buffer_destroy(buf);
    return 1;
  }

  *buffer_ = buf;
  return 0;
}



/* Computing the Levenshtein distance */

int get_levenshtein_distance(buffer const * const buffer_1,
                             buffer const * const buffer_2,
                             size_t * const distance) {
  int ret = 0;
  buffer const * buf_small = NULL;
  buffer const * buf_large = NULL;
  size_t i = 0;
  size_t j = 0;
  size_t t = 0;
  size_t * row_1 = NULL;
  size_t * row_2 = NULL;
  size_t * row_t = NULL;

  if (buffer_1->size < buffer_2->size) {
    buf_small = buffer_1;
    buf_large = buffer_2;
  }
  else {
    buf_small = buffer_2;
    buf_large = buffer_1;
  }
  assert(buf_small->size <= buf_large->size);
  
  ret = size_t_add(&i, buf_small->size, 1); /* (1) */
  if (ret) {
    return ret;
  }
  ret = size_t_mul_aug( &i, sizeof(size_t) );
  if (ret) {
    return ret;
  }
  assert(i);

  row_1 = calloc(1, i); /* indices: 0, ..., buf_small->size */
  if (!row_1) {
    return 1;
  }
  row_2 = calloc(1, i); /* indices: see above */
  if (!row_2) {
    free(row_1);
    return 1;
  }

  for (j = 0; j < buf_small->size + 1; ++j) { /* This is safe, since (1) succeeded. */
    row_1[j] = j;
  }
  for (i = 0; i < buf_large->size; ++i) {
    row_2[0] = i + 1;

    for (j = 0; j < buf_small->size; ++j) {
      t = row_1[j];
      if ( buf_small->pointer[j] !=
           buf_large->pointer[i] ) {
        ++t;
      }
      if (t > row_1[j + 1] + 1) {
          t = row_1[j + 1] + 1;
      }
      if (t > row_2[j] + 1) {
          t = row_2[j] + 1;
      }
      row_2[j + 1] = t;
    }

    row_t = row_1;
    row_1 = row_2;
    row_2 = row_t;
  }

  *distance = row_1[buf_small->size];
  free(row_2);
  free(row_1);
  return 0;
}



/* Computing a lower bound on the Levenshtein distance */

size_t distance(size_t const size_1,
                size_t const size_2) {
  if (size_1 < size_2) {
    return size_2 - size_1;
  }
  return size_1 - size_2;
}

int get_ld_lb(buffer const * const buffer_1,
              buffer const * const buffer_2,
              size_t * const bound) { /* lower bound */
  size_t bound_ = 0;
  int ret = 0;
  size_t i = 0;
  size_t t_1 = 0;
  size_t t_2 = 0;
  size_t freq_buf_1[256] = {0};
  size_t freq_buf_2[256] = {0};

  for (i = 0; i < buffer_1->size; ++i) {
    unsigned char const unsigned_char = *(unsigned char const *)(buffer_1->pointer + i);
    ++freq_buf_1[unsigned_char];
  }
  for (i = 0; i < buffer_2->size; ++i) {
    unsigned char const unsigned_char = *(unsigned char const *)(buffer_2->pointer + i);
    ++freq_buf_2[unsigned_char];
  }

  for (i = 0; i < 256; ++i) {
    t_1 = distance( freq_buf_1[i],
                    freq_buf_2[i] );
    if (bound_ < t_1) {
        bound_ = t_1;
    }
  }

  t_1 = 0;
  for (i = 0; i < 256; ++i) {
    t_2 = distance( freq_buf_1[i],
                    freq_buf_2[i] );
    ret = size_t_add_aug(&t_1, t_2);
    if (ret) {
      return ret;
    }
  }
  t_2 = distance(buffer_1->size,
                 buffer_2->size);
  ret = size_t_add_aug(&t_1, t_2);
  if (ret) {
    return ret;
  }
  if (t_1) {
    t_1 = 1 + (t_1 - 1) / 2;
  }
  if (bound_ < t_1) {
      bound_ = t_1;
  }

  *bound = bound_;
  return 0;
}



/* Computing an upper bound on the Levenshtein distance */

size_t minimum(size_t const size_1,
               size_t const size_2) {
  if (size_1 < size_2) {
    return size_1;
  }
  return size_2;
}

int get_ld_ub(buffer const * const buffer_1,
              buffer const * const buffer_2,
              size_t * const bound) { /* upper bound */
  size_t bound_ = 0;
  int ret = 0;
  size_t buf_1_t = 0;
  size_t buf_2_t = 0;
  buffer sub_buf_1 = {0};
  buffer sub_buf_2 = {0};
  size_t distance = 0;
  
  buf_1_t = buffer_1->size;
  buf_2_t = buffer_2->size;
  sub_buf_1.pointer = buffer_1->pointer;
  sub_buf_2.pointer = buffer_2->pointer;
  sub_buf_1.size = minimum(buf_1_t, 1024);
  sub_buf_2.size = minimum(buf_2_t, 1024);

  while (sub_buf_1.size ||
         sub_buf_2.size) {
    ret = get_levenshtein_distance(&sub_buf_1,
                                   &sub_buf_2,
                                   &distance);
    if (ret) {
      return ret;
    }
    bound_ += distance;
    
    buf_1_t -= sub_buf_1.size;
    buf_2_t -= sub_buf_2.size;
    sub_buf_1.pointer += sub_buf_1.size;
    sub_buf_2.pointer += sub_buf_2.size;
    sub_buf_1.size = minimum(buf_1_t, sub_buf_1.size);
    sub_buf_2.size = minimum(buf_2_t, sub_buf_2.size);
  }

  *bound = bound_;
  return 0;
}



/* Command-line interface */

int main( int argc, char * argv[] ) {
  int ret = 0;
  buffer * buffer_1 = NULL;
  buffer * buffer_2 = NULL;
  size_t max_size = SIZE_MAX;
  size_t printee = 0;

  if ( argc != 4 &&
       argc != 5 ||
       strcmp(argv[1], "-d") &&
       strcmp(argv[1], "-l") &&
       strcmp(argv[1], "-u") ) {
    fprintf(stderr,
      "Usage: program option file1 file2 [read_limit]                                 \n"
      "About:                                                                         \n"
      " This program interprets each file as the bytestring that the file contains;   \n"
      " then, the program prints (a bound on) the Levenshtein distance between the    \n"
      " two bytestrings. The exit status is zero if and only if the program succeeded.\n"
      " Please note: A computation of a bound takes considerably less time than the   \n"
      " computation of the distance, if the files are large.                          \n"
      " For large files, you may want to specify a read_limit. This limits the number \n"
      " of bytes that the program can read from each file; thus, only a prefix of the \n"
      " contained bytestring will be used for the desired computation.                \n"
      "Options:                                                                       \n"
      " -d  Print the Levenshtein distance.                                           \n"
      " -l  Print a lower bound on the distance. (takes the least amount of time)     \n"
      " -u  Print an upper bound.                                                     \n"
    );
    return 1;
  }

  if (argc == 5) {
    ret = size_t_from_string( &max_size, argv[4] );
    if (ret) {
      fprintf(stderr, "Error: Could not accept read_limit.\n");
      return ret;
    }
  }

  ret = buffer_create( argv[2], max_size, &buffer_1 );
  if (ret) {
    fprintf(stderr, "Error: Could not read first file.\n");
    return ret;
  }

  ret = buffer_create( argv[3], max_size, &buffer_2 );
  if (ret) {
    buffer_destroy(buffer_1);
    fprintf(stderr, "Error: Could not read second file.\n");
    return ret;
  }

  switch ( argv[1][1] ) {
  case 'd':
    ret = get_levenshtein_distance(buffer_1, buffer_2, &printee);
    break;
  case 'l':
    ret = get_ld_lb(buffer_1, buffer_2, &printee);
    break;
  case 'u':
    ret = get_ld_ub(buffer_1, buffer_2, &printee);
    break;
  }
  buffer_destroy(buffer_2);
  buffer_destroy(buffer_1);
  if (ret) {
    fprintf(stderr, "Error: Computation failed.\n");
    return ret;
  }

  ret = printf(
#ifdef _MSC_VER
    "%Iu\n",
#else
    "%zu\n",
#endif
    printee);
  if (ret < 0) {
    fprintf(stderr, "Error: Could not print.\n");
    return 1;
  }
  ret = fflush(stdout);
  if (ret) {
    fprintf(stderr, "Error: Could not flush.\n");
    return 1;
  }

  return 0;
}
/* written by Frogger Fioz */
