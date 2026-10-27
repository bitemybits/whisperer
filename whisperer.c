#define _XOPEN_SOURCE 700
#include <microhttpd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

static char ROOT[PATH_MAX], WORKER[PATH_MAX], PYTHON[PATH_MAX], STATUS[PATH_MAX];

static const char *PAGE =
"<!doctype html><meta charset=utf-8><title>Whisperer</title>"
"<style>body{font:16px system-ui;max-width:760px;margin:40px auto;padding:0 16px}label{display:block;margin:16px 0 6px}select,input,button{font:inherit;padding:8px}select{width:100%}.row{display:flex;gap:8px}.row>*{flex:1}#status{white-space:pre-wrap;background:#eee;padding:12px}</style>"
"<h1>Whisperer</h1>"
"<label>Video folder</label><select id=s></select>"
"<label><input id=r type=checkbox> Include subfolders</label>"
"<label>Output</label><label><input name=o type=radio value=same checked> Same folder as each video</label>"
"<label><input name=o type=radio value=dest> Destination folder</label><select id=d></select>"
"<label>Create folder</label><div class=row><select id=p></select><input id=n placeholder='new-folder'><button onclick=mk()>Create</button></div>"
"<p><button onclick=run()>Start transcription</button></p><div id=status>Ready.</div>"
"<script>"
"async function load(){let a=(await (await fetch('/api/dirs')).text()).trim().split('\\n').filter(Boolean);for(let x of ['s','d','p']){let e=document.getElementById(x);e.innerHTML='';a.forEach(v=>e.add(new Option(v||'.',v)))}}"
"async function mk(){let p=pEl.value,n=document.getElementById('n').value.trim();if(!n)return;alert(await (await fetch('/api/mkdir?parent='+encodeURIComponent(p)+'&name='+encodeURIComponent(n),{method:'POST'})).text());await load()}"
"async function run(){let same=document.querySelector('input[name=o]:checked').value==='same';let q=new URLSearchParams({src:s.value,dst:d.value,recursive:r.checked?1:0,same:same?1:0});status.textContent=await (await fetch('/api/run?'+q,{method:'POST'})).text()}"
"async function poll(){try{status.textContent=await (await fetch('/api/status')).text()}catch{}setTimeout(poll,1500)}"
"let s=document.getElementById('s'),d=document.getElementById('d'),pEl=document.getElementById('p'),r=document.getElementById('r'),status=document.getElementById('status');load();poll();"
"</script>";

static enum MHD_Result reply(struct MHD_Connection *c, unsigned code, const char *text, const char *type){
    struct MHD_Response *r=MHD_create_response_from_buffer(strlen(text),(void*)text,MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(r,"Content-Type",type);
    enum MHD_Result x=MHD_queue_response(c,code,r); MHD_destroy_response(r); return x;
}

static int safe_rel(const char *s){ return s && s[0]!='/' && !strstr(s,".."); }
static int path_join(char *out,const char *rel){
    if(!safe_rel(rel)) return 0;
    if(snprintf(out,PATH_MAX,"%s/%s",ROOT,rel)>=PATH_MAX) return 0;
    char real[PATH_MAX]; if(!realpath(out,real)) return 0;
    size_t n=strlen(ROOT); if(strncmp(real,ROOT,n)|| (real[n] && real[n]!='/')) return 0;
    strcpy(out,real); return 1;
}

static void list_dirs_rec(FILE *f,const char *abs,const char *rel){
    fprintf(f,"%s\n",*rel?rel:".");
    DIR *d=opendir(abs); if(!d) return; struct dirent *e;
    while((e=readdir(d))){ if(e->d_name[0]=='.') continue;
        char a[PATH_MAX],r[PATH_MAX]; snprintf(a,sizeof a,"%s/%s",abs,e->d_name); struct stat st;
        if(!lstat(a,&st)&&S_ISDIR(st.st_mode)){ snprintf(r,sizeof r,"%s%s%s",rel,*rel?"/":"",e->d_name); list_dirs_rec(f,a,r); }
    } closedir(d);
}

static enum MHD_Result dirs(struct MHD_Connection *c){
    char *buf=NULL; size_t n=0; FILE *f=open_memstream(&buf,&n); list_dirs_rec(f,ROOT,""); fclose(f);
    enum MHD_Result x=reply(c,200,buf,"text/plain; charset=utf-8"); free(buf); return x;
}

static enum MHD_Result mkdir_api(struct MHD_Connection *c){
    const char *parent=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"parent");
    const char *name=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"name");
    if(!parent||!name||strchr(name,'/')||strstr(name,"..")||!*name) return reply(c,400,"Invalid folder name","text/plain");
    char base[PATH_MAX],p[PATH_MAX]; if(!path_join(base,!strcmp(parent,".")?"":parent)) return reply(c,400,"Invalid parent","text/plain");
    snprintf(p,sizeof p,"%s/%s",base,name); if(mkdir(p,0755)&&errno!=EEXIST) return reply(c,500,strerror(errno),"text/plain");
    return reply(c,200,"Folder created","text/plain");
}

static enum MHD_Result run_api(struct MHD_Connection *c){
    const char *src=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"src"), *dst=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"dst");
    const char *rec=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"recursive"), *same=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"same");
    char a[PATH_MAX],b[PATH_MAX]; if(!src||!path_join(a,!strcmp(src,".")?"":src)) return reply(c,400,"Invalid source","text/plain");
    if(!dst||!path_join(b,!strcmp(dst,".")?"":dst)) return reply(c,400,"Invalid destination","text/plain");
    pid_t p=fork(); if(p<0) return reply(c,500,"Could not start job","text/plain");
    if(!p){ execl(PYTHON,PYTHON,WORKER,a,b,rec?rec:"0",same?same:"1",STATUS,(char*)0); _exit(127); }
    return reply(c,202,"Started","text/plain");
}

static enum MHD_Result status_api(struct MHD_Connection *c){
    FILE *f=fopen(STATUS,"r"); if(!f) return reply(c,200,"Ready.","text/plain"); char buf[4096]; size_t n=fread(buf,1,sizeof(buf)-1,f); fclose(f); buf[n]=0; return reply(c,200,buf,"text/plain; charset=utf-8");
}

static enum MHD_Result handler(void *cls,struct MHD_Connection *c,const char *url,const char *method,const char *ver,const char *up,size_t *upn,void **ctx){
    (void)cls;(void)ver;(void)up;(void)upn;(void)ctx;
    if(!strcmp(url,"/")) return reply(c,200,PAGE,"text/html; charset=utf-8");
    if(!strcmp(url,"/api/dirs")&&!strcmp(method,"GET")) return dirs(c);
    if(!strcmp(url,"/api/mkdir")&&!strcmp(method,"POST")) return mkdir_api(c);
    if(!strcmp(url,"/api/run")&&!strcmp(method,"POST")) return run_api(c);
    if(!strcmp(url,"/api/status")) return status_api(c);
    return reply(c,404,"Not found","text/plain");
}

int main(int argc,char **argv){
    if(argc<2){fprintf(stderr,"Usage: %s ROOT [PORT]\n",argv[0]);return 1;} unsigned port=argc>2?atoi(argv[2]):8080;
    if(!realpath(argv[1],ROOT)){perror("root");return 1;} char cwd[PATH_MAX]; getcwd(cwd,sizeof cwd); snprintf(WORKER,sizeof WORKER,"%s/worker.py",cwd);
    const char *home=getenv("HOME"),*py=getenv("WHISPER_PYTHON"); if(py) snprintf(PYTHON,sizeof PYTHON,"%s",py); else snprintf(PYTHON,sizeof PYTHON,"%s/whisper-env/bin/python",home?home:"/tmp");
    snprintf(STATUS,sizeof STATUS,"/tmp/whisperer-%u.status",(unsigned)getuid());
    struct MHD_Daemon *d=MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD,port,NULL,NULL,&handler,NULL,MHD_OPTION_END);
    if(!d){fprintf(stderr,"Could not listen on %u\n",port);return 1;} printf("Whisperer: http://0.0.0.0:%u  root=%s\n",port,ROOT); getchar(); MHD_stop_daemon(d); return 0;
}
