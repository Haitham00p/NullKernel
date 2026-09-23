#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

#define MAXP 256
#define LIST 8192

static uint32_t my_len(const char *s){ uint32_t n=0; while(s[n]) n++; return n; }
static void my_cpy(char *d, const char *s){ while((*d++ = *s++)); }
static void my_cat(char *d, const char *s){ while(*d) d++; while((*d++ = *s++)); }
static const char *my_rchr(const char *s, char c){ const char *r=0; while(*s){ if(*s==c) r=s; s++; } return r; }

static void strip_slash(char *p){ uint32_t n=my_len(p); while(n>1 && p[n-1]=='/') p[--n]='\0'; }

/* True if path is an existing directory (absolute or relative). */
static int is_dir(const char *path){
    char saved[MAXP]; int ok;
    DiskFsPwd(saved, MAXP);
    ok = DiskFsChdirPath(path);
    DiskFsChdirPath(saved);
    return ok;
}

/* Split path into directory part (no trailing slash) and final name. */
static void split_path(const char *path, char *dir, char *name){
    const char *sl = my_rchr(path, '/');
    if(sl == 0){
        my_cpy(dir, ".");
        my_cpy(name, path);
    } else {
        uint32_t pos = (uint32_t)(sl - path);
        if(pos == 0){
            my_cpy(dir, "/");
            my_cpy(name, path + 1);
        } else {
            uint32_t i;
            for(i = 0; i < pos && i < MAXP - 1U; i++) dir[i] = path[i];
            dir[pos] = '\0';
            my_cpy(name, path + pos + 1);
        }
    }
}

static int copy_file(const char *src, const char *dst){
    char sdir[MAXP], sname[MAXP], ddir[MAXP], dname[MAXP], finaldst[MAXP];
    uint32_t size = 0, got = 0;
    void *buf;
    split_path(src, sdir, sname);
    if(!DiskFsFileSizeInPath(sdir, sname, &size)){
        printf("cp: cannot stat %s\n", src);
        return 0;
    }
    buf = malloc(size ? size : 1);
    if(!buf){ printf("cp: out of memory\n"); return 0; }
    if(!DiskFsReadInPath(sdir, sname, buf, size, &got)){
        printf("cp: read failed %s\n", src);
        free(buf);
        return 0;
    }
    if(is_dir(dst)){
        char tmp[MAXP];
        my_cpy(tmp, dst);
        strip_slash(tmp);
        my_cpy(finaldst, tmp);
        my_cat(finaldst, "/");
        my_cat(finaldst, sname);
    } else {
        my_cpy(finaldst, dst);
    }
    split_path(finaldst, ddir, dname);
    if(!(ddir[0] == '.' && ddir[1] == '\0')) DiskFsMkdirAll(ddir);
    if(DiskFsWriteInPath(ddir, dname, buf, got)){
        free(buf);
        return 1;
    }
    printf("cp: write failed %s\n", dst);
    free(buf);
    return 0;
}

static void copy_tree(const char *srcDir, const char *dstDir){
    char saved[MAXP], list[LIST];
    uint32_t off = 0;
    DiskFsPwd(saved, MAXP);
    if(!DiskFsChdirPath(srcDir)){
        printf("cp: cannot open %s\n", srcDir);
        return;
    }
    list[0] = '\0';
    DiskFsListNames(list, LIST);
    while(list[off]){
        char name[64];
        uint32_t n = 0;
        while(list[off] && list[off] != '\n' && n < 63U){
            name[n++] = list[off++];
        }
        if(list[off] == '\n') off++;
        name[n] = '\0';
        if(!name[0]) continue;
        if(name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
        if(DiskFsIsDirectory(name)){
            char cs[MAXP], cd[MAXP];
            my_cpy(cs, srcDir);
            strip_slash(cs);
            my_cat(cs, "/");
            my_cat(cs, name);
            my_cpy(cd, dstDir);
            strip_slash(cd);
            my_cat(cd, "/");
            my_cat(cd, name);
            DiskFsMkdirAll(cd);
            copy_tree(cs, cd);
        } else {
            uint32_t size = 0, got = 0;
            void *buf;
            char cd[MAXP], ddir[MAXP], dname[MAXP];
            if(!DiskFsFileSize(name, &size)) continue;
            buf = malloc(size ? size : 1);
            if(!buf) continue;
            if(!DiskFsRead(name, buf, size, &got)){
                free(buf);
                continue;
            }
            my_cpy(cd, dstDir);
            strip_slash(cd);
            my_cat(cd, "/");
            my_cat(cd, name);
            split_path(cd, ddir, dname);
            if(!(ddir[0] == '.' && ddir[1] == '\0')) DiskFsMkdirAll(ddir);
            DiskFsWriteInPath(ddir, dname, buf, got);
            free(buf);
        }
    }
    DiskFsChdirPath(saved);
}

void main(void){
    char src[MAXP], dst[MAXP];
    if(ShellArgCount() < 2){
        puts("usage: cp <src> <dst>");
        return;
    }
    ShellArgGet(0, src, MAXP);
    ShellArgGet(1, dst, MAXP);
    if(is_dir(src)){
        char base[MAXP], finaldir[MAXP], tmp[MAXP], srcstrip[MAXP];
        const char *sl;
        uint32_t dlen;
        my_cpy(srcstrip, src);
        strip_slash(srcstrip);
        sl = my_rchr(srcstrip, '/');
        if(sl && sl[1]) my_cpy(base, sl + 1); else my_cpy(base, srcstrip);
        my_cpy(tmp, dst);
        strip_slash(tmp);
        dlen = my_len(dst);
        if((dlen > 0 && dst[dlen - 1U] == '/') || is_dir(dst)){
            my_cpy(finaldir, tmp);
            my_cat(finaldir, "/");
            my_cat(finaldir, base);
        } else {
            my_cpy(finaldir, dst);
        }
        DiskFsMkdirAll(finaldir);
        copy_tree(src, finaldir);
        puts("cp: directory copied");
    } else {
        if(copy_file(src, dst)) puts("cp: copied");
    }
}
