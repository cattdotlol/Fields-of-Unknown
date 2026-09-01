#ifndef TESTS_H
#define TESTS_H

#include <stdbool.h>

void Check(const char *name, bool got, bool expected);

void SuiteWorldgen(void);
void SuiteVitals(void);
void SuiteMushroom(void);
void SuiteInput(void);
void SuiteRat(void);
void SuiteStalker(void);
void SuitePhysics(void);
void SuiteDistrict(void);
void SuiteTree(void);
void SuiteDaylight(void);
void SuiteOcean(void);
void SuiteAquatic(void);

#endif /* TESTS_H */
