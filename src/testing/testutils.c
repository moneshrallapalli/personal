#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "testing.h"
#include "complete.h"

tests_t* s__tests;
unsigned int s__ncategories = 0;


void s__print_results(void) {
  s__restore_stdout();
  fprintf(stderr, "\n  [Test Results for unit testing on PROJECT %d]\n\n", PROJECT);
  for (int i=0; i<s__ncategories; i++) {
    int max_len = 0;
    fprintf(stderr, "%s\n", s__tests[i].category);
    for (int j=0; j<s__tests[i].ntests; j++) {
      int len = strlen(s__tests[i].prompts[j]) + 2;
      max_len = (max_len > len ? max_len : len);
    }
    for (int j=0; j<max_len+5; j++) fprintf(stderr, "━");
    fprintf(stderr, "┳━━┅\n");
    for (int j=0; j<s__tests[i].ntests; j++) {
      fprintf(stderr, "    %-*s │ %s\n", max_len, s__tests[i].prompts[j], s__tests[i].results[j]);
    }
    fprintf(stderr, "\n");
  }
}

void s__initialize_tests(tests_t* tests, int ncategories) {
  s__ncategories = ncategories;
  for (int i=0; i<ncategories; i++) {
    tests[i].ntests = 0;
    for (int j=0; tests[i].prompts[j] && tests[i].prompts[j][0] != '\0'; j++) {
      tests[i].ntests += 1;
      strncpy(tests[i].results[j], "UNTESTED", MAX_RESULT_LEN - 1);
    }
  }
  s__tests = tests;
}

void s__spoof_stdin(char* text, int len) {
  int alt_stdin;
  if ((alt_stdin = open("/tmp", O_TMPFILE | O_RDWR | S_IRUSR | S_IWUSR)) == -1) {
    fprintf(stderr, "Failed to open file `%s`\n", strerror(errno));
    exit(-1);
  }

  if (write(alt_stdin, text, len) == -1) {
    fprintf(stderr, "Failed to write to file `%s`\n", strerror(errno));
    exit(-2);
  }

  lseek(alt_stdin, 0, SEEK_SET);
  if (dup2(alt_stdin, STDIN_FILENO) == -1) {
    fprintf(stderr, "Error in duplicating stdin - `%s`\n", strerror(errno));
    exit(-3);
  }
  close(alt_stdin);
}

void s__dump_stdout(void) {
  freopen("test.stdout", "w", stdout);
  freopen("test.stderr", "w", stderr);
}

void s__restore_stdout(void) {
  freopen("/dev/tty", "w", stdout);
  freopen("/dev/tty", "w", stderr);
}

int assert(int pred, char* slot, const char* err) {
  if (strcmp(slot, "UNTESTED") == 0 || strcmp(slot, "OK") == 0) {
    strncpy(slot, (pred ? "OK" : err), MAX_RESULT_LEN - 1);
  }
  return pred;
}
