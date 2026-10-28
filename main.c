/*
 * ============================================================
 *  RELATION LAB  -  an interactive relation closure engine
 *  CSE 173 Group 9 - North South University
 * ============================================================
 *  A binary relation R on {1..n} is stored as an n x n boolean
 *  matrix. The lab can:
 *    - test 6 properties (with a counter-example when one fails)
 *    - classify R: preorder, equivalence, partial / strict / total order
 *    - build reflexive, symmetric, transitive closures (Warshall trace)
 *    - compute inverse, complement and powers R^k
 *    - find paths, distances, equivalence classes, Hasse diagram
 *    - save / load relations and export a Graphviz graph
 *
 *  Build:  gcc -Wall -Wextra -o relations main.c
 *  Run:    ./relations            (colors on)
 *          ./relations --no-color
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX 12
#define SAVE_FILE "relation.txt"
#define DOT_FILE  "relation.dot"

typedef struct {
    int n;
    int m[MAX][MAX];
} Relation;

/* ---------- colors ---------- */
static const char *RED = "\033[31m", *GRN = "\033[1;32m", *YEL = "\033[33m",
                  *CYN = "\033[36m", *MAG = "\033[35m", *DIM = "\033[2m",
                  *BLD = "\033[1m",  *RST = "\033[0m";

static void colors_off(void) {
    RED = GRN = YEL = CYN = MAG = DIM = BLD = RST = "";
}

/* ---------- safe input ---------- */
static void read_line(char *buf, size_t sz) {
    if (!fgets(buf, (int)sz, stdin)) {
        printf("\nInput closed. Goodbye!\n");
        exit(0);
    }
}

static int read_int(const char *prompt, int lo, int hi) {
    char buf[64];
    int v;
    for (;;) {
        printf("%s", prompt);
        read_line(buf, sizeof buf);
        if (sscanf(buf, "%d", &v) == 1 && v >= lo && v <= hi) return v;
        printf("%s  Please enter a number from %d to %d.%s\n", RED, lo, hi, RST);
    }
}

static void pause_enter(void) {
    char b[16];
    printf("%s  [Press Enter to continue]%s", DIM, RST);
    read_line(b, sizeof b);
}

static void title(const char *t) {
    printf("\n%s=== %s ===%s\n", BLD, t, RST);
}

/* ---------- basic relation helpers ---------- */
static void rel_init(Relation *r, int n) {
    r->n = n;
    memset(r->m, 0, sizeof r->m);
}

static int count_pairs(const Relation *r) {
    int c = 0;
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++) c += r->m[i][j];
    return c;
}

static void print_pairs(const Relation *r) {
    int first = 1;
    printf("{ ");
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j]) {
                printf("%s(%d,%d)", first ? "" : ", ", i + 1, j + 1);
                first = 0;
            }
    printf("%s }\n", first ? "empty" : "");
}

/* prints the matrix; entries that are 1 here but 0 in `base` are highlighted */
static void print_matrix(const Relation *r, const char *name, const Relation *base) {
    printf("\n  %s\n      ", name);
    for (int j = 0; j < r->n; j++) printf("%2d ", j + 1);
    printf("\n      ");
    for (int j = 0; j < r->n; j++) printf("---");
    printf("\n");
    for (int i = 0; i < r->n; i++) {
        printf("  %2d |", i + 1);
        for (int j = 0; j < r->n; j++) {
            if (r->m[i][j] && base && !base->m[i][j]) printf("%s%2d%s ", GRN, 1, RST);
            else if (r->m[i][j])                      printf("%s%2d%s ", CYN, 1, RST);
            else                                      printf("%s%2d%s ", DIM, 0, RST);
        }
        printf("\n");
    }
}

static void print_new_pairs(const Relation *after, const Relation *before) {
    int any = 0;
    printf("  New pairs: ");
    for (int i = 0; i < after->n; i++)
        for (int j = 0; j < after->n; j++)
            if (after->m[i][j] && !before->m[i][j]) {
                printf("%s(%d,%d)%s ", GRN, i + 1, j + 1, RST);
                any = 1;
            }
    printf("%s\n", any ? "" : "none");
}

/* ---------- property tests (each returns 1 if it holds, else fills witness) ---------- */
static int is_reflexive(const Relation *r, int w[3]) {
    for (int i = 0; i < r->n; i++)
        if (!r->m[i][i]) { w[0] = i; return 0; }
    return 1;
}
static int is_irreflexive(const Relation *r, int w[3]) {
    for (int i = 0; i < r->n; i++)
        if (r->m[i][i]) { w[0] = i; return 0; }
    return 1;
}
static int is_symmetric(const Relation *r, int w[3]) {
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j] && !r->m[j][i]) { w[0] = i; w[1] = j; return 0; }
    return 1;
}
static int is_antisymmetric(const Relation *r, int w[3]) {
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (i != j && r->m[i][j] && r->m[j][i]) { w[0] = i; w[1] = j; return 0; }
    return 1;
}
static int is_asymmetric(const Relation *r, int w[3]) {
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j] && r->m[j][i]) { w[0] = i; w[1] = j; return 0; }
    return 1;
}
static int is_transitive(const Relation *r, int w[3]) {
    for (int i = 0; i < r->n; i++)
        for (int k = 0; k < r->n; k++)
            if (r->m[i][k])
                for (int j = 0; j < r->n; j++)
                    if (r->m[k][j] && !r->m[i][j]) { w[0] = i; w[1] = k; w[2] = j; return 0; }
    return 1;
}
static int is_total(const Relation *r) {
    for (int i = 0; i < r->n; i++)
        for (int j = i + 1; j < r->n; j++)
            if (!r->m[i][j] && !r->m[j][i]) return 0;
    return 1;
}

static void row(const char *name, int ok, const char *why) {
    printf("  %-14s %s%s%s  %s%s%s\n", name,
           ok ? GRN : RED, ok ? "[YES]" : "[NO ]", RST, DIM, why, RST);
}

static void analyze(const Relation *r) {
    int w[3] = {0, 0, 0};
    char why[128];
    title("PROPERTY ANALYSIS");

    int refl = is_reflexive(r, w);
    snprintf(why, sizeof why, refl ? "every (a,a) is present" : "(%d,%d) is missing", w[0] + 1, w[0] + 1);
    row("Reflexive", refl, why);

    int irr = is_irreflexive(r, w);
    snprintf(why, sizeof why, irr ? "no (a,a) is present" : "(%d,%d) is present", w[0] + 1, w[0] + 1);
    row("Irreflexive", irr, why);

    int sym = is_symmetric(r, w);
    snprintf(why, sizeof why, sym ? "every (a,b) has (b,a)" : "(%d,%d) is in R but (%d,%d) is not", w[0] + 1, w[1] + 1, w[1] + 1, w[0] + 1);
    row("Symmetric", sym, why);

    int anti = is_antisymmetric(r, w);
    snprintf(why, sizeof why, anti ? "no two different elements point to each other" : "(%d,%d) and (%d,%d) are both in R", w[0] + 1, w[1] + 1, w[1] + 1, w[0] + 1);
    row("Antisymmetric", anti, why);

    int asym = is_asymmetric(r, w);
    snprintf(why, sizeof why, asym ? "no (a,b) has (b,a)" : "(%d,%d) and (%d,%d) are both in R", w[0] + 1, w[1] + 1, w[1] + 1, w[0] + 1);
    row("Asymmetric", asym, why);

    int tra = is_transitive(r, w);
    snprintf(why, sizeof why, tra ? "every 2-step path has a shortcut" : "(%d,%d) and (%d,%d) are in R but (%d,%d) is not", w[0] + 1, w[1] + 1, w[1] + 1, w[2] + 1, w[0] + 1, w[2] + 1);
    row("Transitive", tra, why);

    printf("\n  %sClassification%s\n", BLD, RST);
    int found = 0;
    if (refl && tra)           { printf("   %s*%s Preorder\n", YEL, RST); found = 1; }
    if (refl && sym && tra)    { printf("   %s*%s Equivalence relation\n", YEL, RST); found = 1; }
    if (refl && anti && tra)   { printf("   %s*%s Partial order (poset)\n", YEL, RST); found = 1; }
    if (irr && tra)            { printf("   %s*%s Strict partial order\n", YEL, RST); found = 1; }
    if (refl && anti && tra && is_total(r)) { printf("   %s*%s Total (linear) order\n", YEL, RST); found = 1; }
    if (!found) printf("   %s*%s None of the standard types\n", DIM, RST);
}

/* ---------- closures ---------- */
static Relation reflexive_closure(const Relation *r) {
    Relation c = *r;
    for (int i = 0; i < c.n; i++) c.m[i][i] = 1;
    return c;
}

static Relation symmetric_closure(const Relation *r) {
    Relation c = *r;
    for (int i = 0; i < c.n; i++)
        for (int j = 0; j < c.n; j++)
            if (r->m[i][j]) c.m[j][i] = 1;
    return c;
}

/* trace: print each step; stepmode: wait for Enter between steps */
static Relation transitive_closure(const Relation *r, int trace, int stepmode) {
    Relation c = *r;
    for (int k = 0; k < c.n; k++) {
        Relation before = c;
        for (int i = 0; i < c.n; i++)
            for (int j = 0; j < c.n; j++)
                c.m[i][j] = c.m[i][j] || (c.m[i][k] && c.m[k][j]);
        if (trace) {
            char t[96];
            snprintf(t, sizeof t, "Warshall step k = %d  (paths may pass through nodes 1..%d)", k + 1, k + 1);
            print_matrix(&c, t, &before);
            print_new_pairs(&c, &before);
            if (stepmode && k < c.n - 1) pause_enter();
        }
    }
    return c;
}

static Relation equivalence_closure(const Relation *r) {
    Relation s = symmetric_closure(r);
    Relation t = transitive_closure(&s, 0, 0);
    return reflexive_closure(&t);
}

/* ---------- operations ---------- */
static Relation inverse(const Relation *r) {
    Relation c = *r;
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++) c.m[i][j] = r->m[j][i];
    return c;
}

static Relation complement(const Relation *r) {
    Relation c = *r;
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++) c.m[i][j] = !r->m[i][j];
    return c;
}

/* boolean matrix product: (a o b) */
static Relation compose(const Relation *a, const Relation *b) {
    Relation c;
    rel_init(&c, a->n);
    for (int i = 0; i < a->n; i++)
        for (int k = 0; k < a->n; k++)
            if (a->m[i][k])
                for (int j = 0; j < a->n; j++)
                    if (b->m[k][j]) c.m[i][j] = 1;
    return c;
}

/* ---------- paths (BFS) ---------- */
static void bfs(const Relation *r, int s, int dist[], int par[]) {
    int q[MAX * MAX], head = 0, tail = 0;
    for (int i = 0; i < r->n; i++) { dist[i] = -1; par[i] = -1; }
    for (int j = 0; j < r->n; j++)
        if (r->m[s][j] && dist[j] < 0) { dist[j] = 1; par[j] = s; q[tail++] = j; }
    while (head < tail) {
        int u = q[head++];
        for (int v = 0; v < r->n; v++)
            if (r->m[u][v] && dist[v] < 0) { dist[v] = dist[u] + 1; par[v] = u; q[tail++] = v; }
    }
}

static void find_path(const Relation *r, int s, int t) {
    int dist[MAX], par[MAX], path[MAX + 2], len = 0;
    bfs(r, s, dist, par);
    if (dist[t] < 0) {
        printf("  %sNo path from %d to %d%s  ->  (%d,%d) is NOT in the transitive closure.\n", RED, s + 1, t + 1, RST, s + 1, t + 1);
        return;
    }
    path[len++] = t;
    for (int cur = par[t]; cur != s; cur = par[cur]) path[len++] = cur;
    path[len++] = s;
    printf("  %sShortest path (%d step%s):%s ", GRN, dist[t], dist[t] == 1 ? "" : "s", RST);
    for (int i = len - 1; i >= 0; i--) printf("%d%s", path[i] + 1, i ? " -> " : "\n");
    printf("  So (%d,%d) belongs to the transitive closure.\n", s + 1, t + 1);
}

static void distance_matrix(const Relation *r) {
    int dist[MAX], par[MAX];
    printf("\n  Shortest path length from row to column ( . = unreachable )\n      ");
    for (int j = 0; j < r->n; j++) printf("%2d ", j + 1);
    printf("\n      ");
    for (int j = 0; j < r->n; j++) printf("---");
    printf("\n");
    for (int i = 0; i < r->n; i++) {
        bfs(r, i, dist, par);
        printf("  %2d |", i + 1);
        for (int j = 0; j < r->n; j++) {
            if (dist[j] < 0) printf("%s . %s", DIM, RST);
            else             printf("%s%2d%s ", CYN, dist[j], RST);
        }
        printf("\n");
    }
    printf("  %sA number d at (i,j) means (i,j) first appears in R^d.%s\n", DIM, RST);
}

/* ---------- structure tools ---------- */
static void equivalence_classes(const Relation *r) {
    int w[3];
    if (!(is_reflexive(r, w) && is_symmetric(r, w) && is_transitive(r, w))) {
        printf("  %sR is not an equivalence relation, so it has no equivalence classes.%s\n", RED, RST);
        printf("  Tip: use menu 6 and choose 'Equivalence closure' to build one.\n");
        return;
    }
    int seen[MAX] = {0}, classes = 0;
    printf("  Partition of the set:\n");
    for (int i = 0; i < r->n; i++) {
        if (seen[i]) continue;
        printf("   %s[%d]%s = { ", GRN, i + 1, RST);
        int first = 1;
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j]) { printf("%s%d", first ? "" : ", ", j + 1); seen[j] = 1; first = 0; }
        printf(" }\n");
        classes++;
    }
    printf("  %d equivalence class%s.\n", classes, classes == 1 ? "" : "es");
}

static void hasse(const Relation *r) {
    int w[3];
    if (!(is_reflexive(r, w) && is_antisymmetric(r, w) && is_transitive(r, w))) {
        printf("  %sR is not a partial order (needs reflexive + antisymmetric + transitive).%s\n", RED, RST);
        return;
    }
    printf("  Covering pairs (edges of the Hasse diagram), a below b:\n");
    int any = 0;
    for (int a = 0; a < r->n; a++)
        for (int b = 0; b < r->n; b++) {
            if (a == b || !r->m[a][b]) continue;
            int cover = 1;
            for (int c = 0; c < r->n && cover; c++)
                if (c != a && c != b && r->m[a][c] && r->m[c][b]) cover = 0;
            if (cover) { printf("   %d --< %d\n", a + 1, b + 1); any = 1; }
        }
    if (!any) printf("   (none)\n");
    printf("  Minimal elements: ");
    for (int a = 0; a < r->n; a++) {
        int min = 1;
        for (int x = 0; x < r->n; x++) if (x != a && r->m[x][a]) min = 0;
        if (min) printf("%d ", a + 1);
    }
    printf("\n  Maximal elements: ");
    for (int a = 0; a < r->n; a++) {
        int mx = 1;
        for (int x = 0; x < r->n; x++) if (x != a && r->m[a][x]) mx = 0;
        if (mx) printf("%d ", a + 1);
    }
    printf("\n  Total order (chain)? %s\n", is_total(r) ? "YES" : "NO - some elements are incomparable");
}

/* ---------- show ---------- */
static void show_relation(const Relation *r) {
    title("CURRENT RELATION");
    printf("  Set A = {1..%d}   |R| = %d pair%s\n  R = ", r->n, count_pairs(r), count_pairs(r) == 1 ? "" : "s");
    print_pairs(r);
    print_matrix(r, "Matrix M_R:", NULL);
    printf("\n  Element  out-degree  in-degree\n");
    for (int i = 0; i < r->n; i++) {
        int o = 0, in = 0;
        for (int j = 0; j < r->n; j++) { o += r->m[i][j]; in += r->m[j][i]; }
        printf("  %4d     %5d       %5d\n", i + 1, o, in);
    }
}

/* ---------- input / presets ---------- */
static void enter_pairs(Relation *r) {
    int n = read_int("  Size of the set n (1-12): ", 1, MAX);
    rel_init(r, n);
    printf("  Type pairs as 'a b' (meaning a R b). Type 0 to finish.\n");
    char buf[64];
    for (;;) {
        printf("  pair> ");
        read_line(buf, sizeof buf);
        int a, b;
        int got = sscanf(buf, "%d %d", &a, &b);
        if (got >= 1 && a == 0) break;
        if (got != 2 || a < 1 || a > n || b < 1 || b > n) {
            printf("%s  Enter two numbers from 1 to %d, or 0 to finish.%s\n", RED, n, RST);
            continue;
        }
        r->m[a - 1][b - 1] = 1;
    }
}

static void enter_matrix(Relation *r) {
    int n = read_int("  Size of the set n (1-12): ", 1, MAX);
    rel_init(r, n);
    printf("  Enter each row as %d numbers (0 or 1) separated by spaces.\n", n);
    for (int i = 0; i < n; i++) {
        for (;;) {
            printf("  row %2d> ", i + 1);
            char buf[256];
            read_line(buf, sizeof buf);
            int vals[MAX], cnt = 0, ok = 1;
            char *p = buf, *end;
            while (cnt < n) {
                long v = strtol(p, &end, 10);
                if (end == p || (v != 0 && v != 1)) { ok = 0; break; }
                vals[cnt++] = (int)v;
                p = end;
            }
            if (ok && cnt == n) {
                for (int j = 0; j < n; j++) r->m[i][j] = vals[j];
                break;
            }
            printf("%s  Need exactly %d values, each 0 or 1.%s\n", RED, n, RST);
        }
    }
}

static void load_preset(Relation *r) {
    printf("\n  1) Chain         1->2->3->4\n"
           "  2) Cycle         1->2->3->1\n"
           "  3) Congruence    a ~ b when a = b (mod 3) on {1..6}   [equivalence]\n"
           "  4) Divisibility  a | b on {1..6}                       [partial order]\n"
           "  5) Less-or-equal a <= b on {1..5}                      [total order]\n"
           "  6) Random        5 elements\n");
    int c = read_int("  Choose preset (1-6): ", 1, 6);
    switch (c) {
    case 1: rel_init(r, 4); r->m[0][1] = r->m[1][2] = r->m[2][3] = 1; break;
    case 2: rel_init(r, 3); r->m[0][1] = r->m[1][2] = r->m[2][0] = 1; break;
    case 3: rel_init(r, 6);
            for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) r->m[i][j] = ((i - j) % 3 == 0);
            break;
    case 4: rel_init(r, 6);
            for (int i = 1; i <= 6; i++) for (int j = 1; j <= 6; j++) r->m[i - 1][j - 1] = (j % i == 0);
            break;
    case 5: rel_init(r, 5);
            for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) r->m[i][j] = (i <= j);
            break;
    default: rel_init(r, 5); srand((unsigned)time(NULL));
            for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) r->m[i][j] = (rand() % 100) < 25;
            break;
    }
}

/* ---------- files ---------- */
static void save_relation(const Relation *r) {
    FILE *f = fopen(SAVE_FILE, "w");
    if (!f) { printf("%s  Could not write %s%s\n", RED, SAVE_FILE, RST); return; }
    fprintf(f, "%d\n", r->n);
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++)
            if (r->m[i][j]) fprintf(f, "%d %d\n", i + 1, j + 1);
    fclose(f);
    printf("  %sSaved to %s%s\n", GRN, SAVE_FILE, RST);
}

static void load_relation(Relation *r) {
    FILE *f = fopen(SAVE_FILE, "r");
    if (!f) { printf("%s  %s not found. Save a relation first.%s\n", RED, SAVE_FILE, RST); return; }
    int n;
    if (fscanf(f, "%d", &n) != 1 || n < 1 || n > MAX) {
        printf("%s  File is not valid.%s\n", RED, RST);
        fclose(f);
        return;
    }
    rel_init(r, n);
    int a, b;
    while (fscanf(f, "%d %d", &a, &b) == 2)
        if (a >= 1 && a <= n && b >= 1 && b <= n) r->m[a - 1][b - 1] = 1;
    fclose(f);
    printf("  %sLoaded from %s%s\n", GRN, SAVE_FILE, RST);
}

/* original edges solid, edges added by the transitive closure dashed red */
static void export_dot(const Relation *r) {
    Relation t = transitive_closure(r, 0, 0);
    FILE *f = fopen(DOT_FILE, "w");
    if (!f) { printf("%s  Could not write %s%s\n", RED, DOT_FILE, RST); return; }
    fprintf(f, "digraph R {\n  rankdir=LR;\n  node [shape=circle, style=filled, fillcolor=lightblue];\n");
    for (int i = 0; i < r->n; i++) fprintf(f, "  %d;\n", i + 1);
    for (int i = 0; i < r->n; i++)
        for (int j = 0; j < r->n; j++) {
            if (r->m[i][j]) fprintf(f, "  %d -> %d;\n", i + 1, j + 1);
            else if (t.m[i][j]) fprintf(f, "  %d -> %d [style=dashed, color=red];\n", i + 1, j + 1);
        }
    fprintf(f, "}\n");
    fclose(f);
    printf("  %sExported %s%s  (solid = original, dashed red = added by transitive closure)\n", GRN, DOT_FILE, RST);
    printf("  Make a picture with:  dot -Tpng %s -o relation.png\n", DOT_FILE);
}

/* ---------- menus ---------- */
static void closures_menu(const Relation *r) {
    printf("\n  1) Reflexive closure       r(R) = R U I\n"
           "  2) Symmetric closure       s(R) = R U R^-1\n"
           "  3) Transitive closure      t(R)  (Warshall, auto trace)\n"
           "  4) Transitive closure      (Warshall, step by step)\n"
           "  5) Equivalence closure     t(s(r(R)))\n"
           "  6) Show all closures\n");
    int c = read_int("  Choose (1-6): ", 1, 6);
    Relation out;
    switch (c) {
    case 1: out = reflexive_closure(r);
            title("REFLEXIVE CLOSURE"); printf("  r(R) = "); print_pairs(&out);
            print_matrix(&out, "Matrix (green = added):", r); break;
    case 2: out = symmetric_closure(r);
            title("SYMMETRIC CLOSURE"); printf("  s(R) = "); print_pairs(&out);
            print_matrix(&out, "Matrix (green = added):", r); break;
    case 3:
    case 4: title("TRANSITIVE CLOSURE (WARSHALL)");
            out = transitive_closure(r, 1, c == 4);
            printf("\n  t(R) = "); print_pairs(&out); break;
    case 5: out = equivalence_closure(r);
            title("SMALLEST EQUIVALENCE RELATION CONTAINING R");
            printf("  "); print_pairs(&out);
            print_matrix(&out, "Matrix (green = added):", r);
            equivalence_classes(&out); break;
    default: {
            Relation a = reflexive_closure(r), b = symmetric_closure(r), d = transitive_closure(r, 0, 0);
            title("ALL CLOSURES");
            printf("  r(R) = "); print_pairs(&a);
            printf("  s(R) = "); print_pairs(&b);
            printf("  t(R) = "); print_pairs(&d);
            printf("\n  Closures grow R by +%d, +%d, +%d pairs.\n",
                   count_pairs(&a) - count_pairs(r), count_pairs(&b) - count_pairs(r), count_pairs(&d) - count_pairs(r));
            break; }
    }
}

static void operations_menu(const Relation *r) {
    printf("\n  1) Inverse       R^-1\n"
           "  2) Complement    not R\n"
           "  3) Power         R^k\n");
    int c = read_int("  Choose (1-3): ", 1, 3);
    Relation out;
    if (c == 1) {
        out = inverse(r);
        title("INVERSE  R^-1"); print_pairs(&out); print_matrix(&out, "Matrix (transpose of M_R):", NULL);
    } else if (c == 2) {
        out = complement(r);
        title("COMPLEMENT"); print_pairs(&out); print_matrix(&out, "Matrix (every bit flipped):", NULL);
    } else {
        int k = read_int("  k (1-10): ", 1, 10);
        out = *r;
        for (int i = 1; i < k; i++) out = compose(&out, r);
        char t[96];
        snprintf(t, sizeof t, "POWER  R^%d  (pairs reachable in exactly %d steps)", k, k);
        title(t); print_pairs(&out); print_matrix(&out, "Matrix:", NULL);
    }
}

static void paths_menu(const Relation *r) {
    printf("\n  1) Is there a path from a to b?\n  2) Distance table (all pairs)\n");
    int c = read_int("  Choose (1-2): ", 1, 2);
    if (c == 1) {
        int a = read_int("  From element: ", 1, r->n);
        int b = read_int("  To element:   ", 1, r->n);
        find_path(r, a - 1, b - 1);
    } else {
        distance_matrix(r);
    }
}

static void glossary(void) {
    title("GLOSSARY OF TERMS");
    printf("  Relation          a set of ordered pairs (a,b) read 'a is related to b'\n"
           "  Reflexive         every a is related to itself\n"
           "  Irreflexive       no a is related to itself\n"
           "  Symmetric         (a,b) in R forces (b,a) in R\n"
           "  Antisymmetric     (a,b) and (b,a) in R only when a = b\n"
           "  Asymmetric        (a,b) in R forbids (b,a) in R\n"
           "  Transitive        (a,b) and (b,c) in R force (a,c) in R\n"
           "  Closure           the smallest relation containing R with the property\n"
           "  Warshall          O(n^3) algorithm that builds the transitive closure\n"
           "  Equivalence       reflexive + symmetric + transitive; splits A into classes\n"
           "  Partial order     reflexive + antisymmetric + transitive\n"
           "  Total order       partial order where every two elements are comparable\n"
           "  Hasse diagram     partial order drawn with only the covering pairs\n"
           "  Inverse R^-1      all pairs flipped: (b,a) for each (a,b)\n"
           "  Power R^k         pairs joined by a walk of exactly k steps\n");
}

static void banner(const Relation *r) {
    int w[3];
    printf("\n%s+------------------------------------------------------+%s\n", MAG, RST);
    printf("%s|           R E L A T I O N   L A B   (CSE 173)         |%s\n", MAG, RST);
    printf("%s+------------------------------------------------------+%s\n", MAG, RST);
    printf("  Current: n = %d, |R| = %d   [%s%s%s%s%s]\n", r->n, count_pairs(r),
           is_reflexive(r, w) ? "refl " : "", is_symmetric(r, w) ? "sym " : "",
           is_antisymmetric(r, w) ? "anti " : "", is_transitive(r, w) ? "trans" : "",
           "");
    printf("   1) Enter relation as pairs      8) Paths and distances\n"
           "   2) Enter relation as matrix     9) Equivalence classes\n"
           "   3) Load a preset example       10) Hasse diagram / order tools\n"
           "   4) Show current relation       11) Save / load file\n"
           "   5) Analyze properties          12) Export Graphviz graph\n"
           "   6) Closures (Warshall)         13) Glossary of terms\n"
           "   7) Inverse, complement, power   0) Exit\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--no-color") == 0) colors_off();

    Relation r;
    rel_init(&r, 4);
    r.m[0][1] = r.m[1][2] = r.m[2][3] = 1;   /* start with the chain so every menu works */

    for (;;) {
        banner(&r);
        int c = read_int("\n  Your choice: ", 0, 13);
        switch (c) {
        case 0:  printf("\n  Goodbye!\n"); return 0;
        case 1:  enter_pairs(&r);  show_relation(&r); break;
        case 2:  enter_matrix(&r); show_relation(&r); break;
        case 3:  load_preset(&r);  show_relation(&r); break;
        case 4:  show_relation(&r); break;
        case 5:  analyze(&r); break;
        case 6:  closures_menu(&r); break;
        case 7:  operations_menu(&r); break;
        case 8:  paths_menu(&r); break;
        case 9:  title("EQUIVALENCE CLASSES"); equivalence_classes(&r); break;
        case 10: title("ORDER TOOLS"); hasse(&r); break;
        case 11: {
            int s = read_int("  1) Save   2) Load : ", 1, 2);
            if (s == 1) save_relation(&r); else load_relation(&r);
            break; }
        case 12: export_dot(&r); break;
        default: glossary(); break;
        }
        pause_enter();
    }
}
