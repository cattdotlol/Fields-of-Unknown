#ifndef TESTS_H
#define TESTS_H

#include <stdbool.h>

void Check(const char *name, bool got, bool expected);

void SuiteWorldgen(void);
void SuiteVitals(void);
void SuiteMushroom(void);

#endif /* TESTS_H */
