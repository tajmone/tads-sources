#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "diff.h"

/* array item comparison callback */
static int cmp_line(const void *a0, const void *b0, void *ctx)
{
    const char *a = (const char *)a0;
    const char *b = (const char *)b0;

    return (strcmp(a, b));
}

/* array index callback */
static const void *idx_line(const void *a0, int idx, void *ctx)
{
    const char **a = (const char **)a0;
    return a[idx];
}

/* file line array */
typedef struct line_array line_array;
struct line_array
{
    /* line array */
    char **lines;

    /* number of lines */
    int cnt;

    /* overall file buffer */
    char buf[1];
};

/* load a file */
static line_array *load_file(const char *fname)
{
    FILE *fp;
    long len;
    line_array *arr;
    int i;
    char *p;

    /* open the file */
    if ((fp = fopen(fname, "r")) == 0)
        return 0;

    /* get its length */
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* create a buffer for it; leave space for a final null terminator */
    arr = (line_array *)malloc(sizeof(line_array) + len + 1);
    if (arr == 0)
    {
        fclose(fp);
        return 0;
    }

    /* load it; the loaded length could shrink due to newline translation */
    len = fread(arr->buf, 1, len, fp);
    if (ferror(fp))
    {
        free(arr);
        fclose(fp);
        return 0;
    }

    /* null-terminate at the end of the buffer */
    arr->buf[len] = '\0';

    /* done with the file */
    fclose(fp);

    /* count the lines */
    for (i = 1, p = arr->buf ; p < arr->buf + len ; ++i, ++p)
    {
        /* find the next newline */
        for ( ; p < arr->buf + len && *p != '\n' ; ++p) ;

        /* if we reached the end of the buffer, we're done */
        if (p >= arr->buf + len)
            break;
    }

    /* allocate the line array */
    if ((arr->lines = (char **)malloc(i * sizeof(arr->lines[0]))) == 0)
    {
        free(arr);
        return 0;
    }

    /* set it up */
    arr->cnt = i;
    arr->lines[0] = arr->buf;
    for (i = 1, p = arr->buf ; p < arr->buf + len && i < arr->cnt ; ++i)
    {
        /* find the next newline; convert any nulls along the way to spaces */
        for ( ; p < arr->buf + len && *p != '\n' ; ++p)
        {
            if (*p == '\0')
                *p = ' ';
        }

        /* null-terminate here */
        *p = '\0';

        /* if we've reached the end of the buffer, we're done */
        if (p >= arr->buf + len)
            break;

        /* set the next line pointer */
        arr->lines[i] = ++p;
    }

    /* return the line array */
    return arr;
}

/* delete a line array */
static void del_line_array(line_array *arr)
{
    /* delete the lines */
    free(arr->lines);

    /* delete the overall structure */
    free(arr);
}

static void errexit(const char *msg, ...)
{
    va_list va;

    va_start(va, msg);
    vprintf(msg, va);
    printf("\n");
    va_end(va);

    exit(1);
}

void main(int argc, char **argv)
{
    line_array *a, *b;
    int i, sn, d;
    struct varray *ses = varray_new(sizeof(struct diff_edit), 0);
    int la, lb;

    /* check arguments */
    if (argc != 3)
        errexit("usage: difftest file1 file2");

    /* load the two files */
    if ((a = load_file(argv[1])) == 0)
        errexit("error loading %s", argv[1]);
    if ((b = load_file(argv[2])) == 0)
    {
        del_line_array(a);
        errexit("error loading %s", argv[2]);
    }

    /* diff the files */
    d = diff(a->lines, 0, a->cnt, b->lines, 0, b->cnt,
             idx_line, cmp_line, 0, 0, ses, &sn, 0);

    /* report the diffs */
    for (i = 0, la = lb = 1 ; i < sn ; ++i)
    {
        struct diff_edit *e = varray_get(ses, i);
        int j;

        /* consolidate del+ins into a 'change' report */
        if (i + 1 < sn && e->op == DIFF_DELETE && (e+1)->op == DIFF_INSERT)
        {
            if (e->len == 1)
                printf("%dc", la);
            else
                printf("%d,%dc", la, la + e->len - 1);
            if ((e+1)->len == 1)
                printf("%d\n", lb);
            else
                printf("%d,%d\n", lb, lb + (e+1)->len - 1);

            for (j = 0 ; j < e->len ; ++j)
                printf("< %s\n", a->lines[e->off + j]);

            printf("---\n");

            for (j = 0 ; j < (e+1)->len ; ++j)
                printf("> %s\n", b->lines[(e+1)->off + j]);

            la += e->len;
            lb += (e+1)->len;
            ++i;
            continue;
        }

        switch (e->op)
        {
        case DIFF_MATCH:
            /* advance both line numbers past the match */
            la += e->len;
            lb += e->len;
            break;

        case DIFF_DELETE:
            /* deleting from first file */
            if (e->len == 1)
                printf("%dd%d\n", la, lb);
            else
                printf("%d,%dd%d\n", la, la + e->len - 1, lb);

            for (j = 0 ; j < e->len ; ++j)
                printf("< %s\n", a->lines[e->off + j]);

            la += e->len;
            break;

        case DIFF_INSERT:
            /* inserting from second file */
            if (e->len == 1)
                printf("%da%d\n", la - 1, lb);
            else
                printf("%da%d,%d\n", la - 1, lb, lb + e->len - 1);

            for (j = 0 ; j < e->len ; ++j)
                printf("> %s\n", b->lines[e->off + j]);

            lb += e->len;
            break;
        }
    }

    /* delete the session array */
    varray_del(ses);

    /* delete the line arrays */
    del_line_array(a);
    del_line_array(b);
}
