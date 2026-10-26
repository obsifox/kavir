/* E1 backend probe — C version (compiled by gcc).
 *
 * The identical algorithm is hand-written in QBE IL in probe.ssa.
 * Deterministic, integer-only, one checksum line on stdout.
 * Pre-declared criteria in benchmarks/methodology.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define N 512u
#define SIEVE_N 10000000ULL

static int cmp_u32(const void *x, const void *y)
{
    uint32_t u = *(const uint32_t *)x;
    uint32_t v = *(const uint32_t *)y;
    return (u > v) - (u < v);
}

int main(void)
{
    unsigned char *sieve = calloc(SIEVE_N, 1);
    uint64_t count = 0, sum = 0;
    for (uint64_t i = 2; i < SIEVE_N; i++)
        if (!sieve[i])
            for (uint64_t j = i * i; j < SIEVE_N; j += i)
                sieve[j] = 1;
    for (uint64_t i = 2; i < SIEVE_N; i++)
        if (!sieve[i]) { count++; sum += i; }

    uint32_t *a = calloc((size_t)N * N, 4);
    uint32_t *b = calloc((size_t)N * N, 4);
    uint32_t *c = calloc((size_t)N * N, 4);
    for (unsigned i = 0; i < N * N; i++) {
        a[i] = (uint32_t)(i * 2654435761u);
        b[i] = (uint32_t)(i * 40503u + 1);
    }
    for (unsigned i = 0; i < N; i++)
        for (unsigned j = 0; j < N; j++) {
            uint32_t acc = 0;
            for (unsigned k = 0; k < N; k++)
                acc += a[i * N + k] * b[k * N + j]; /* wrapping uint32 */
            c[i * N + j] = acc;
        }

    uint32_t corner[16];
    for (unsigned i = 0; i < 16; i++)
        corner[i] = c[i * 33] ^ c[N * N - 1 - i * 33];
    qsort(corner, 16, 4, cmp_u32);

    printf("%llu %llu %u %u\n",
           (unsigned long long)count, (unsigned long long)sum,
           (unsigned)corner[0], (unsigned)corner[15]);

    free(sieve); free(a); free(b); free(c);
    return 0;
}
