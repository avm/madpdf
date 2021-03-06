#define _GNU_SOURCE

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <expat.h>
#include "settings.h"
#include "madpdf.h"

typedef struct
{
    int in_panning;
    int in_panning_overlap;
    int in_padding;
    int in_padding_left;
    int in_padding_right;
    int in_zoom;
    int in_zoom_increment;
} parsenav;


static progsettings *settings=NULL;
static parsenav *parsinginfo=NULL;

static void free_parsinginfo();

void save_settings(const char *filename)
{
    int filehandle=open(filename, O_WRONLY|O_TRUNC|O_CREAT,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    FILE *f = fdopen(filehandle, "w");
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<settings>\n\n<panning>\n", f);
    fprintf(f, "\t<overlapmin>%d</overlapmin>\n", settings->pan_overlap);

    fputs("</panning>\n\n<zoom>\n", f);
    fprintf(f, "\t<increment>%d</increment>\n", settings->zoominc);

    fputs("</zoom>\n\n<trimpadding>\n", f);
    fprintf(f, "\t<left>%d</left>\n", settings->ltrimpad);
    fprintf(f, "\t<right>%d</right>\n", settings->rtrimpad);
    fputs("</trimpadding>\n\n</settings>\n", f);

    fclose(f);
}

progsettings *get_settings()
{
    return settings;
}

void handlestart(void *userData,const XML_Char *name,const XML_Char **atts)
{
    
    
    if(strcmp(name,"panning")==0)
        parsinginfo->in_panning=1;
    else if(strcmp(name,"overlapmin")==0 && parsinginfo->in_panning)
        parsinginfo->in_panning_overlap=1;
    else if(strcmp(name,"zoom")==0)
        parsinginfo->in_zoom=1;
    else if(strcmp(name,"increment")==0 && parsinginfo->in_zoom)
        parsinginfo->in_zoom_increment=1;
    else if(strcmp(name,"trimpadding")==0)
        parsinginfo->in_padding=1;
    else if(strcmp(name,"left")==0 && parsinginfo->in_padding)
        parsinginfo->in_padding_left=1;
    else if(strcmp(name,"right")==0 && parsinginfo->in_padding)
        parsinginfo->in_padding_right=1;
}

void handleend(void *userData,const XML_Char *name)
{
    if(strcmp(name,"panning")==0)
        parsinginfo->in_panning=0;
    else if(strcmp(name,"overlapmin")==0 && parsinginfo->in_panning)
        parsinginfo->in_panning_overlap=0;
    else if(strcmp(name,"zoom")==0)
        parsinginfo->in_zoom=0;
    else if(strcmp(name,"increment")==0 && parsinginfo->in_zoom)
        parsinginfo->in_zoom_increment=0;
    else if(strcmp(name,"trimpadding")==0)
        parsinginfo->in_padding=0;
    else if(strcmp(name,"left")==0 && parsinginfo->in_padding)
        parsinginfo->in_padding_left=0;
    else if(strcmp(name,"right")==0 && parsinginfo->in_padding)
        parsinginfo->in_padding_right=0;
}


void handlechar(void *userData,const XML_Char *s,int len)
{
    if(len>0)
    {
        char *temp2 = strndup(s, len);

        if(parsinginfo->in_panning_overlap)
            settings->pan_overlap=(int)strtol(temp2,NULL,10);
        else if(parsinginfo->in_padding_left)
            settings->ltrimpad=(int)strtol(temp2,NULL,10);
        else if(parsinginfo->in_padding_right)
            settings->rtrimpad=(int)strtol(temp2,NULL,10);

        free(temp2);
    }
}




void init_settings()
{
    free_settings();
    settings=(progsettings*)malloc(sizeof(progsettings));
    settings->hpan=100;
    settings->vpan=100;
    settings->pan_overlap=10;
    settings->ltrimpad=0;
    settings->rtrimpad=0;
    settings->zoominc=15;
}
void init_parsinginfo()
{
    parsinginfo=(parsenav*)malloc(sizeof(parsenav));
    parsinginfo->in_panning=0;
    parsinginfo->in_panning_overlap=0;
    parsinginfo->in_padding=0;
    parsinginfo->in_padding_left=0;
    parsinginfo->in_padding_right=0;
    parsinginfo->in_zoom=0;
    parsinginfo->in_zoom_increment=0;
}

void load_settings(const char *filename)
{
    struct stat stat_p;
    int filehandle;
    char *buffer;
    long nread;
    XML_Parser myparse;
    
    init_settings();
    init_parsinginfo();
    
    
    myparse=XML_ParserCreate("UTF-8");
    XML_UseParserAsHandlerArg(myparse);
    XML_SetElementHandler(myparse,handlestart,handleend);
    XML_SetCharacterDataHandler(myparse,handlechar);
    
    
    
    
    filehandle=open(filename,O_RDONLY);
    if(filehandle == -1) {
        XML_ParserFree(myparse);
        return;
    }

    stat(filename,&stat_p);
    buffer=(char *)malloc(stat_p.st_size);
    nread=read(filehandle,(void *)buffer,stat_p.st_size);
    XML_Parse(myparse,buffer,nread,1);
    
    
    free(buffer);
    free_parsinginfo();
    close(filehandle);
    
    XML_ParserFree(myparse);

}


void free_settings()
{
    if(settings!=NULL)
    {
        free(settings);
        settings=NULL;
    }

}

static void free_parsinginfo()
{
    if(parsinginfo!=NULL)
    {
        free(parsinginfo);
        parsinginfo=NULL;
    }

}

/* vim:set et sw=4: */
