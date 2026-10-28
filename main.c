/*
 * Relation Closure Engine
 * Represents a binary relation on {1..n} as an n x n boolean matrix
 * and computes its reflexive, symmetric and transitive closures.
 *
 * Build:  gcc -Wall -Wextra -o relations main.c
 * Run:    ./relations          (interactive)
 *         ./relations --demo   (built-in example)
 */
#include <stdio.h>
#include <string.h>

#define MAX 10

typedef struct {
    int n;
    int m[MAX][MAX];
} Relation;

/* ---------- display ---------- */

void print_matrix(const Relation *r, const char *title) {
    printf("\n%s\n    ", title);
    for (int j = 0; j < r->n; j++) printf("%2d ", j + 1);
    printf("\n    ");
    for (int j = 0; j < r->n; j++) printf("---");
    printf("\n");
    for (int i = 0; i < r->n; i++) {
        printf("%2d |", i + 1);
        for (int j = 0; j < r->n; j++) printf("%2d ", r->m[i][j]);
        printf("\n");
    }
}

void print_pairs(const Relation *r) {
    int first = 1;
    printf("{ ");
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j]) {
                printf("%s(%d,%d)", first ? "" : ", ", i + 1, j + 1);
                first = 0;
            }
    printf(" }\n");
}

/* ---------- property tests ---------- */

int is_reflexive(const Relation *r) {
    for (int i = 0; i < r->n; i++)
        if (!r->m[i][i]) return 0;
    return 1;
}

int is_symmetric(const Relation *r) {
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j] != r->m[j][i]) return 0;
    return 1;
}

int is_transitive(const Relation *r) {
    for (int i = 0; i < r->n; i++)
        for (int k = 0; k < r->n; k++)
            if (r->m[i][k])
                for (int j = 0; j < r->n; j++)
                    if (r->m[k][j] && !r->m[i][j]) return 0;
    return 1;
}

void print_properties(const Relation *r) {
    printf("\nProperties:\n");
    printf("  Reflexive  : %s\n", is_reflexive(r)  ? "YES" : "NO");
    printf("  Symmetric  : %s\n", is_symmetric(r)  ? "YES" : "NO");
    printf("  Transitive : %s\n", is_transitive(r) ? "YES" : "NO");
    if (is_reflexive(r) && is_symmetric(r) && is_transitive(r))
        printf("  >> Equivalence relation\n");
}

/* ---------- closures ---------- */

/* R U I : add every (i,i) */
Relation reflexive_closure(const Relation *r) {
    Relation c = *r;
    for (int i = 0; i < c.n; i++) c.m[i][i] = 1;
    return c;
}

/* R U R^-1 : if (i,j) then (j,i) */
Relation symmetric_closure(const Relation *r) {
    Relation c = *r;
    for (int i = 0; i < c.n; i++)
        for (int j = 0; j < c.n; j++)
            if (r->m[i][j]) c.m[j][i] = 1;
    return c;
}

/* Warshall: R+ = R U R^2 U ... U R^n  in O(n^3) */
Relation transitive_closure(const Relation *r, int verbose) {
    Relation c = *r;
    for (int k = 0; k < c.n; k++) {
        for (int i = 0; i < c.n; i++)
            for (int j = 0; j < c.n; j++)
                c.m[i][j] = c.m[i][j] || (c.m[i][k] && c.m[k][j]);
        if (verbose) {
            char title[64];
            snprintf(title, sizeof title, "Warshall step k = %d (paths through %d allowed)", k + 1, k + 1);
            print_matrix(&c, title);
        }
    }
    return c;
}

/* ---------- input ---------- */

void read_relation(Relation *r) {
    printf("Size of set n (1-%d): ", MAX);
    while (scanf("%d", &r->n) != 1 || r->n < 1 || r->n > MAX) {
        printf("Invalid. Enter n between 1 and %d: ", MAX);
        int ch; while ((ch = getchar()) != '\n' && ch != EOF);
    }
    memset(r->m, 0, sizeof r->m);
    printf("Enter pairs 'a b' meaning a R b. Enter 0 0 to stop.\n");
    int a, b;
    while (scanf("%d %d", &a, &b) == 2 && !(a == 0 && b == 0)) {
        if (a < 1 || a > r->n || b < 1 || b > r->n) printf("  Out of range, ignored.\n");
        else r->m[a - 1][b - 1] = 1;
    }
}

void load_demo(Relation *r) {
    r->n = 4;
    memset(r->m, 0, sizeof r->m);
    r->m[0][1] = 1;  /* 1 -> 2 */
    r->m[1][2] = 1;  /* 2 -> 3 */
    r->m[2][3] = 1;  /* 3 -> 4 */
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    Relation r;
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) load_demo(&r);
    else read_relation(&r);

    printf("\n=== ORIGINAL RELATION ===\nR = ");
    print_pairs(&r);
    print_matrix(&r, "Matrix M_R:");
    print_properties(&r);

    Relation rc = reflexive_closure(&r);
    printf("\n=== REFLEXIVE CLOSURE  (R U I) ===\nr(R) = ");
    print_pairs(&rc);
    print_matrix(&rc, "Matrix:");

    Relation sc = symmetric_closure(&r);
    printf("\n=== SYMMETRIC CLOSURE  (R U R^-1) ===\ns(R) = ");
    print_pairs(&sc);
    print_matrix(&sc, "Matrix:");

    printf("\n=== TRANSITIVE CLOSURE  (Warshall) ===");
    Relation tc = transitive_closure(&r, 1);
    printf("\nt(R) = ");
    print_pairs(&tc);
    print_matrix(&tc, "Final matrix:");
    print_properties(&tc);

    Relation eq = transitive_closure(&sc, 0);
    eq = reflexive_closure(&eq);
    printf("\n=== SMALLEST EQUIVALENCE RELATION CONTAINING R ===\n");
    print_pairs(&eq);
    print_properties(&eq);
    return 0;
}
