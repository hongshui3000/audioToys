struct SleefRNG *SleefRNG_init(uint64_t seed);
void SleefRNG_dispose(struct SleefRNG *thiz);

uint64_t SleefRNG_next(struct SleefRNG *thiz, int nbits);
uint64_t SleefRNG_next64(struct SleefRNG *thiz);

void SleefRNG_nextBytes(struct SleefRNG *thiz, uint8_t *ptr, size_t z);
double SleefRNG_nextDouble(struct SleefRNG *thiz);
double SleefRNG_nextRectangularDouble(struct SleefRNG *thiz, double min, double max);

void SleefRNG_fillRectangularDouble(struct SleefRNG *thiz, double *ptr, size_t z, double min, double max);
double SleefRNG_nextTriangularDouble(struct SleefRNG *thiz, double peak);
void SleefRNG_fillTriangularDouble(struct SleefRNG *thiz, double *ptr, size_t z, double peak);
double SleefRNG_nextTwoLevelDouble(struct SleefRNG *thiz, double peak);
void SleefRNG_fillTwoLevelDouble(struct SleefRNG *thiz, double *ptr, size_t z, double peak);
