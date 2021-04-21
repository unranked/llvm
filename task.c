#define VECSIZE 1000
#define VECTYPE int // double

typedef struct {
    VECTYPE values[VECSIZE];
    _Bool null[VECSIZE];
} Vector;

void addv(Vector *vec1, Vector *vec2, Vector *result) {
    for (int i = 0; i < VECSIZE; i++) {
        if (!vec1->null[i] && !vec2->null[i]) {
            result->values[i] = vec1->values[i] + vec2->values[i];
            result->null[i] = 0;
        } else {
            result->null[i] = 1;
        }
    }
}