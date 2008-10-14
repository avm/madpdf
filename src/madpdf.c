/***************************************************************************
 *   Copyright (C) 2008 by Marc Lajoie   *
 *   marc@gatherer   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>
#include <Ewl.h>
#include <Epdf.h>
#include <ewl_pdf.h>
#include "madpdf.h"
#include "Dialogs.h"
#include "settings.h"
//#include "opt_dlg.h"

Ewl_Widget *win = NULL;
Ewl_Widget *pdfwidget = NULL;
Ewl_Widget *scrollpane = NULL;
Ewl_Widget *trimpane = NULL;
Ewl_Widget *menu=NULL;
Ewl_Widget *statlabel1=NULL;
Ewl_Widget *statlabel2=NULL;
Ewl_Widget *goto_entry;
double curscale=1.0;
//for later, margin cuttoffs
int fitmode=0;
double leftmarge=0;
double rightmarge=0;


/* Returns edje theme file name (pointer to static buffer). */
const char* get_theme_file()
{
    static const char system_theme[] = "/usr/share/madpdf/madpdf.edj";
    static const char rel_theme[] = "./themes/edje/madpdf.edj";

    if(0 == access(rel_theme, R_OK))
        return rel_theme;

    if(0 == access(system_theme, R_OK))
        return system_theme;

    fprintf(stderr, "Unable to find any theme. Silly me.\n");
    exit(1);
}

void update_main_app()
{
    resize_and_rescale(curscale);
    update_statusbar();    
    
}

static double pan_inc(double doc, double win)
{
    if(doc <= win)
        // no need to scroll
        return 0.0;

    /* Now panning comes into the picture.
     * We have this setting, the "minimum pan overlap" (%).
     * Let us prepare the absolute (minimum) overlap amount, in pixels. */
    double overlap_fraction = (double)get_settings()->pan_overlap / 100.0;
    double overlap_amount = win * overlap_fraction;

    /* The question is: how many pannings do we need per page,
     * given the doc and win sizes and the overlap amount? */
    double pan_steps = ceil((doc - overlap_amount) / (win - overlap_amount));
    assert(pan_steps > 1.0);

    /* To move to next step (we can do that pan_steps - 1 times),
     * advance a (pan_steps - 1)th part of what is outside the window. */
    return 1.0 / (pan_steps - 1);
}

double get_horizontal_pan_inc()
{
    double ws=(double)CURRENT_W(scrollpane);
    double wt=(double)CURRENT_W(trimpane);
    return pan_inc(wt, ws);
    
}
double get_vertical_pan_inc()
{
    double hs=(double)CURRENT_H(scrollpane);
    double ht=(double)CURRENT_H(trimpane);
    return pan_inc(ht, hs);
}

int translate_key(Ewl_Event_Key_Down* e)
{
    const char* k = e->base.keyname;

    if (!strcmp(k, "Escape"))
        return K_ESCAPE;
    if (!strcmp(k, "Return"))
        return K_RETURN;
    if (isdigit(k[0]) && !k[1])
        return k[0] - '0';
    return K_UNKNOWN;
}

int file_exists(const char *filename)
{
    struct stat stFileInfo;
    int intStat = stat(filename,&stFileInfo);
    if(intStat == 0) {
        return 1;
    }
    return 0;
}

int get_left_margin()
{
    int imgw,imgh;
    int hor,ver;
    int firstpix;
    int *imgptr=evas_object_image_data_get(EWL_PDF(pdfwidget)->image,0);
    evas_object_image_size_get(EWL_PDF(pdfwidget)->image,&imgw,&imgh);
    firstpix=imgptr[0];
    for(hor=0;hor<imgw;hor++)
    {
        
        for(ver=0;ver<imgh;ver++)
        {
            
            if(imgptr[ver*imgw+hor]!=firstpix)
                return hor;
        }
    }
    return 0;
}
int get_right_margin()
{
    int imgw,imgh;
    int hor,ver;
    int firstpix;
    int *imgptr=evas_object_image_data_get(EWL_PDF(pdfwidget)->image,0);
    evas_object_image_size_get(EWL_PDF(pdfwidget)->image,&imgw,&imgh);
    firstpix=imgptr[imgw-1];
    for(hor=imgw-1;hor>=0;hor--)
    {

        for(ver=0;ver<imgh;ver++)
        {
            if(imgptr[ver*imgw+hor]!=firstpix)
                return imgw-1-hor;
        }
    }
    return 0;
}
int get_top_margin()
{
    int imgw,imgh;
    int hor,ver;
    int firstpix;
    int *imgptr=evas_object_image_data_get(EWL_PDF(pdfwidget)->image,0);
    evas_object_image_size_get(EWL_PDF(pdfwidget)->image,&imgw,&imgh);
    firstpix=imgptr[0];
    for(ver=0;ver<imgh;ver++)
    {
        for(hor=0;hor<imgw;hor++)
        {
        
            if(imgptr[ver*imgw+hor]!=firstpix)
                return ver;
        }
    }
    return 0;
}
int get_bottom_margin()
{
    int imgw,imgh;
    int hor,ver;
    int firstpix;
    int curpix;
    int *imgptr=evas_object_image_data_get(EWL_PDF(pdfwidget)->image,0);
    evas_object_image_size_get(EWL_PDF(pdfwidget)->image,&imgw,&imgh);
    firstpix=imgptr[(imgh-1)*imgw];
    for(ver=imgh-1;ver>=0;ver--)
    {
        
        
        for(hor=0;hor<imgw;hor++)
        {
        
            if(imgptr[ver*imgw+hor]!=firstpix)
                return imgh-1-ver;
        }
    }
    return 0;
}

#define SWAP(type, a, b) \
do { \
    type tmp = *a; \
    *a = *b; \
    *b = tmp; \
} while(0)

static void page_size_get(int *width, int *height)
{
    epdf_page_size_get(EWL_PDF(pdfwidget)->pdf_page, width, height);
    switch(epdf_page_orientation_get(EWL_PDF(pdfwidget)->pdf_page)) {
            case EPDF_PAGE_ORIENTATION_LANDSCAPE:
            case EPDF_PAGE_ORIENTATION_SEASCAPE:
                SWAP(int, width, height);
    }
}

static void page_scale_get(double *hscale, double *vscale)
{
    ewl_pdf_scale_get(EWL_PDF(pdfwidget), hscale, vscale);
    switch(epdf_page_orientation_get(EWL_PDF(pdfwidget)->pdf_page)) {
            case EPDF_PAGE_ORIENTATION_LANDSCAPE:
            case EPDF_PAGE_ORIENTATION_SEASCAPE:
                SWAP(double, hscale, vscale);
    }
}

void calculate_margins()
{
    int docwidth,docheight;
    double hscale,vscale;
    page_size_get(&docwidth, &docheight);
    page_scale_get(&hscale, &vscale);
    leftmarge=((double)get_left_margin())/floor(((double)docwidth)*hscale);
    rightmarge=((double)get_right_margin())/floor(((double)docwidth)*hscale);
    fprintf(stderr,"%f %f",leftmarge,rightmarge);
}
void resize_and_rescale(double scale)
{
    int docwidth,docheight;
    double docscale;
    double ltrimpct=0.0,rtrimpct=0.0;
    
    int sp_inner_w = CURRENT_W(scrollpane) - INSET_HORIZONTAL(scrollpane) -
        PADDING_HORIZONTAL(scrollpane);
    int sp_inner_h = CURRENT_H(scrollpane) - INSET_VERTICAL(scrollpane) -
        PADDING_VERTICAL(scrollpane);
        
    page_size_get(&docwidth, &docheight);

    int orientation_horizontal;
    switch(epdf_page_orientation_get(EWL_PDF(pdfwidget)->pdf_page)) {
        case EPDF_PAGE_ORIENTATION_PORTRAIT:
        case EPDF_PAGE_ORIENTATION_UPSIDEDOWN:
            orientation_horizontal = 0;
            break;
        case EPDF_PAGE_ORIENTATION_LANDSCAPE:
        case EPDF_PAGE_ORIENTATION_SEASCAPE:
            orientation_horizontal = 1;
    }

    if(fitmode==0) {
        if(orientation_horizontal)
            docscale = ((double)sp_inner_h)/((double)docheight)*scale;
        else // orientation vertical
            docscale = ((double)sp_inner_w)/((double)docwidth)*scale;
    }
    else if(fitmode==1)
    {
        ltrimpct=((double)get_settings()->ltrimpad)/((double)docwidth);
        rtrimpct=((double)get_settings()->rtrimpad)/((double)docwidth);
        docscale=((double)sp_inner_w)/((1.0-leftmarge+ltrimpct-rightmarge+rtrimpct)*((double)docwidth))*scale;
        
    }
    ewl_pdf_scale_set(EWL_PDF(pdfwidget),docscale,docscale);
    
    ewl_object_custom_w_set(EWL_OBJECT(pdfwidget),floor(((double)docwidth)*docscale));
    ewl_object_custom_h_set(EWL_OBJECT(pdfwidget),floor(((double)docheight)*docscale));
    ewl_widget_configure(pdfwidget);
    
    if(orientation_horizontal) {
        ewl_object_custom_w_set(EWL_OBJECT(trimpane),
                floor(((double)docwidth)*docscale));
        ewl_object_custom_h_set(EWL_OBJECT(trimpane),
                floor(((double)sp_inner_h)*scale));
    } else {
        ewl_object_custom_w_set(EWL_OBJECT(trimpane),
                floor(((double)sp_inner_w)*scale));
        ewl_object_custom_h_set(EWL_OBJECT(trimpane),
                floor(((double)docheight)*docscale));
    }
    ewl_object_position_request(EWL_OBJECT(trimpane),0,0);
    

    ewl_widget_configure(trimpane);
    ewl_widget_configure(scrollpane);
    if(fitmode==0)
        ewl_scrollpane_hscrollbar_value_set(EWL_SCROLLPANE(trimpane),0.0);
    else if(fitmode==1)
        ewl_scrollpane_hscrollbar_value_set(EWL_SCROLLPANE(trimpane),(leftmarge-ltrimpct)/(leftmarge-ltrimpct+rightmarge-rtrimpct));
}
void update_statusbar()
{
    static char statlabel1str[100];
    sprintf(statlabel1str,"MadPDF (OK for Menu)");
    ewl_label_text_set(EWL_LABEL(statlabel1),statlabel1str);
    
    int curpage,totalpage;
    curpage=ewl_pdf_page_get(EWL_PDF(pdfwidget))+1;
    totalpage=epdf_document_page_count_get(ewl_pdf_pdf_document_get(EWL_PDF(pdfwidget)));
    
    static char statlabel2str[100];
    sprintf(statlabel2str,"pg: %d/%d  zoom: %d%%  ",curpage,totalpage,(int)round(curscale*100.0));
    double hpos,vpos;
    int leftarr=0,rightarr=0,downarr=0,uparr=0;
    if(CURRENT_W(trimpane)>CURRENT_W(scrollpane))
    {
        hpos=ewl_scrollpane_hscrollbar_value_get(EWL_SCROLLPANE(scrollpane));
        if(hpos>0.0)
        {
            leftarr=1;
        }
        if(hpos<1.0)
        {
            rightarr=1;    
        }
        
    }
    if(CURRENT_H(trimpane)>CURRENT_H(scrollpane))
    {
        vpos=ewl_scrollpane_vscrollbar_value_get(EWL_SCROLLPANE(scrollpane));
        if(vpos>0.0)
        {
            uparr=1;
        }
        if(vpos<1.0)
        {
            downarr=1;    
        }
        
    }
    if(leftarr||rightarr||downarr||uparr)
        strcat(statlabel2str,"pan: ");
    else
        strcat(statlabel2str,"pan: none");
    if(leftarr)
        strcat(statlabel2str,"←");
    if(rightarr)
        strcat(statlabel2str,"→");
    if(downarr)
        strcat(statlabel2str,"↓");
    if(uparr)
        strcat(statlabel2str,"↑");
    ewl_label_text_set(EWL_LABEL(statlabel2),statlabel2str);
}

static double clamp(double min, double val, double max)
{
    return fmax(min, fmin(val, max));
}

/* Calculate a new scrollbar position, snapping to edges */
static double scroll_pos_add(double position, double amount)
{
    double res = clamp(0.0, position + amount, 1.0);
    if(amount > 0 && (1.0 - res) < amount * 0.1)
        res = 1.0;
    if(amount < 0 && res < -amount * 0.1)
        res = 0.0;
    return res;
}

static void move_hscrollbar(Ewl_Scrollpane *s, double amount)
{
    ewl_scrollpane_hscrollbar_value_set(s,
            scroll_pos_add(ewl_scrollpane_hscrollbar_value_get(s), amount));
    update_statusbar();
}
static void move_vscrollbar(Ewl_Scrollpane *s, double amount)
{
    ewl_scrollpane_vscrollbar_value_set(s,
            scroll_pos_add(ewl_scrollpane_vscrollbar_value_get(s), amount));
    update_statusbar();
}

/* Renormalize scroll position (0.0..1.0) to scroll location (-1.0..1.0) */
static double from_scroll_position(double position)
{
    return position * 2.0 - 1.0;
}
/* Inverse of from_scroll_position() */
static double to_scroll_position(double location)
{
    return (location + 1.0) / 2.0;
}

/* Swap scrollbar values after an orientation change.
 * direction == -1 -> turn clockwise, 1 -> counter-clockwise. */
static void turn_scrollbars(Ewl_Scrollpane *s, int direction)
{
    double h = from_scroll_position(ewl_scrollpane_hscrollbar_value_get(s));
    double v = from_scroll_position(ewl_scrollpane_vscrollbar_value_get(s));

    double new_h =  v * direction;
    double new_v = -h * direction;

    ewl_scrollpane_hscrollbar_value_set(s, to_scroll_position(new_h));
    ewl_scrollpane_vscrollbar_value_set(s, to_scroll_position(new_v));
    update_statusbar();
}

static void change_orientation(Ewl_Widget *pdfwidget)
{
    Epdf_Page_Orientation o = ewl_pdf_orientation_get(EWL_PDF(pdfwidget));
    o = EPDF_PAGE_ORIENTATION_LANDSCAPE + EPDF_PAGE_ORIENTATION_PORTRAIT - o;
    ewl_pdf_orientation_set(EWL_PDF(pdfwidget), o);

    turn_scrollbars(EWL_SCROLLPANE(scrollpane),
            2 * (EPDF_PAGE_ORIENTATION_PORTRAIT == o) - 1);
}

void cb_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Widget *curwidget;
    Ewl_Event_Key_Down *e;
    e = (Ewl_Event_Key_Down*)ev;
    int k = translate_key(e);
    
    curwidget=ewl_widget_name_find("pdfwidget");
    switch(k)
    {
    case 0:
        ewl_pdf_page_next(EWL_PDF(curwidget));
        resize_and_rescale(curscale);
        break;
    case 9:
        ewl_pdf_page_previous(EWL_PDF(curwidget));
        resize_and_rescale(curscale);
        break;
    case 8:
        curscale+=((double)get_settings()->zoominc)/100.0;
        resize_and_rescale(curscale);
        break;
    case 7:
        curscale-=((double)get_settings()->zoominc)/100.0;
        resize_and_rescale(curscale);
        break;
    case 6:
        change_orientation(curwidget);
        resize_and_rescale(curscale);
        break;
    case 1:
        move_hscrollbar(EWL_SCROLLPANE(scrollpane), -get_horizontal_pan_inc());
        break;
    case 2:
        move_hscrollbar(EWL_SCROLLPANE(scrollpane), get_horizontal_pan_inc());
        break;
    case 3:
        move_vscrollbar(EWL_SCROLLPANE(scrollpane), get_vertical_pan_inc());
        break;    
    case 4:
        move_vscrollbar(EWL_SCROLLPANE(scrollpane), -get_vertical_pan_inc());
        break;
    case 5:
        if(fitmode==0)
            fitmode=1;
        else if(fitmode==1)
            fitmode=0;
        calculate_margins();
        resize_and_rescale(curscale);
        break;
    case K_RETURN:
        ewl_widget_show(menu);
        ewl_widget_focus_send(menu);
        break;
    case K_ESCAPE:
        ewl_main_quit();
        break;
    default:
        return;
    }
    
    
    
}
void cb_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Widget *curwidget;
    char temp[50];
    Ewl_Event_Key_Down *e;
    e = (Ewl_Event_Key_Down*)ev;
    int k = translate_key(e);
    
    switch(k)
    {
    case 1:
        sprintf(temp,"menuitem1");
        curwidget=ewl_widget_name_find(temp);
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
        int curpage=ewl_pdf_page_get(EWL_PDF(pdfwidget))+1;
        ewl_text_text_set(EWL_TEXT(goto_entry),"");
        ewl_widget_focus_send(goto_entry);
        break;
    case 2:
        /*ewl_widget_hide(menu);
        opt_dlg_init();    
        ewl_window_transient_for(EWL_WINDOW(opt_dlg_widget_get()),EWL_WINDOW(win));
        ewl_widget_show(opt_dlg_widget_get());
        ewl_widget_focus_send(opt_dlg_widget_get());*/
        ewl_widget_hide(menu);
        OptionsDialog();
        
        break;
    case K_ESCAPE:
        ewl_widget_hide(menu);
        break;
    default:
        return;
    }
    
}
void cb_goto_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Widget *curwidget;
    char temp[50];
    Ewl_Event_Key_Down *e;
    e = (Ewl_Event_Key_Down*)ev;
    int k = translate_key(e);
    int page,totalpages;
    switch(k)
    {
    case K_RETURN:
        ewl_widget_hide(menu);
        totalpages=epdf_document_page_count_get(ewl_pdf_pdf_document_get(EWL_PDF(pdfwidget)));
        page=(int)strtol(ewl_text_text_get(EWL_TEXT(goto_entry)),NULL,10);
        sprintf(temp,"menuitem1");
        curwidget=ewl_widget_name_find(temp);
        ewl_menu_collapse(EWL_MENU(curwidget));
        if(page>0&&page<=totalpages)
            ewl_pdf_page_set(EWL_PDF(pdfwidget),page-1);
        break;
    case K_ESCAPE:
        sprintf(temp,"menuitem1");
        curwidget=ewl_widget_name_find(temp);
        ewl_menu_collapse(EWL_MENU(curwidget));
        ewl_widget_focus_send(menu);
        break;

    default:
        return;
    }
    
}
void cb_scrollpane_revealed(Ewl_Widget *w, void *ev, void *data)
{
    resize_and_rescale(curscale);
    update_statusbar();
}
void destroy_cb ( Ewl_Widget *w, void *event, void *data )
{
    ewl_widget_destroy ( w );
    ewl_main_quit();
}
void cb_pdfwidget_resized ( Ewl_Widget *w, void *event, void *data )
{
    update_statusbar();
    
}

int main ( int argc, char ** argv )
{	

    
    Ewl_Widget *vbox=NULL;
    Ewl_Widget *statbar=NULL;
    char *homedir;
    char *configfile;
    if(argc<2)
        return 1;
    
    if ( !ewl_init ( &argc, argv ) )
    {
        return 1;
    }

    //setlocale(LC_ALL, "");
    //textdomain("elementpdf");
    ewl_theme_theme_set(get_theme_file());
    
    homedir=getenv("HOME");
    configfile=(char *)calloc(strlen(homedir)+21 + 1, sizeof(char));
    strcat(configfile,homedir);
    strcat(configfile,"/.madpdf");
    
    if(!file_exists(configfile))
    {
        mkdir (configfile,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
        
    }
    
    strcat(configfile,"/settings.xml");
    
    
    
    load_settings(configfile);
    
    win = ewl_window_new();
    ewl_window_title_set ( EWL_WINDOW ( win ), "MadPDF" );
    ewl_window_name_set ( EWL_WINDOW ( win ), "madpdf" );
    ewl_window_class_set ( EWL_WINDOW ( win ), "MadPDF" );
    ewl_object_size_request ( EWL_OBJECT ( win ), 600, 800 );
    ewl_callback_append ( win, EWL_CALLBACK_DELETE_WINDOW, destroy_cb, NULL );
    ewl_callback_append(win, EWL_CALLBACK_KEY_DOWN, cb_key_down, NULL);
    ewl_widget_name_set(win,"mainwindow");
    ewl_widget_show ( win );
 
    vbox=ewl_vbox_new();
    ewl_container_child_append(EWL_CONTAINER(win),vbox);
    ewl_object_fill_policy_set(EWL_OBJECT(vbox), EWL_FLAG_FILL_FILL);
    ewl_widget_show(vbox);
    
    scrollpane=ewl_scrollpane_new();
    ewl_container_child_append(EWL_CONTAINER(vbox),scrollpane);
    ewl_callback_append(scrollpane,EWL_CALLBACK_REVEAL,cb_scrollpane_revealed,NULL);
    ewl_scrollpane_hscrollbar_flag_set(EWL_SCROLLPANE(scrollpane),EWL_SCROLLPANE_FLAG_ALWAYS_HIDDEN);
    ewl_scrollpane_vscrollbar_flag_set(EWL_SCROLLPANE(scrollpane),EWL_SCROLLPANE_FLAG_ALWAYS_HIDDEN);
    ewl_widget_show(scrollpane);
    
    trimpane=ewl_scrollpane_new();
    ewl_container_child_append(EWL_CONTAINER(scrollpane),trimpane);
    ewl_object_alignment_set(EWL_OBJECT(trimpane),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_TOP);
    ewl_scrollpane_hscrollbar_flag_set(EWL_SCROLLPANE(trimpane),EWL_SCROLLPANE_FLAG_ALWAYS_HIDDEN);
    ewl_scrollpane_vscrollbar_flag_set(EWL_SCROLLPANE(trimpane),EWL_SCROLLPANE_FLAG_ALWAYS_HIDDEN);
    ewl_widget_show(trimpane);
    
    statbar=ewl_hbox_new();
    ewl_container_child_append(EWL_CONTAINER(vbox),statbar);
    ewl_theme_data_str_set(EWL_WIDGET(statbar),"/hbox/group","ewl/menu/oi_menu");
    ewl_object_fill_policy_set(EWL_OBJECT(statbar),EWL_FLAG_FILL_HFILL|EWL_FLAG_FILL_VSHRINKABLE);
    ewl_widget_show(statbar);
    
    statlabel1=ewl_label_new();   
    ewl_container_child_append(EWL_CONTAINER(statbar),statlabel1);
    ewl_theme_data_str_set(EWL_WIDGET(statlabel1),"/label/group","ewl/oi_statbar_label_left");
    ewl_theme_data_str_set(EWL_WIDGET(statlabel1),"/label/textpart","ewl/oi_statbar_label_left/text");
    ewl_object_fill_policy_set(EWL_OBJECT(statlabel1),EWL_FLAG_FILL_HSHRINKABLE);
    ewl_widget_show(statlabel1);
    
    statlabel2=ewl_label_new();   
    ewl_container_child_append(EWL_CONTAINER(statbar),statlabel2);
    ewl_theme_data_str_set(EWL_WIDGET(statlabel2),"/label/group","ewl/oi_statbar_label_right");
    ewl_theme_data_str_set(EWL_WIDGET(statlabel2),"/label/textpart","ewl/oi_statbar_label_right/text");
    ewl_object_fill_policy_set(EWL_OBJECT(statlabel2),EWL_FLAG_FILL_HFILL);
    ewl_widget_show(statlabel2);
    
    
    pdfwidget = ewl_pdf_new();
    ewl_pdf_file_set (EWL_PDF (pdfwidget), argv[1]);
    ewl_container_child_append(EWL_CONTAINER(trimpane),pdfwidget);
    ewl_object_alignment_set(EWL_OBJECT(pdfwidget),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_TOP);
    ewl_widget_name_set(pdfwidget,"pdfwidget");
    ewl_callback_append (pdfwidget, EWL_CALLBACK_CONFIGURE, cb_pdfwidget_resized, NULL );
    ewl_widget_show (pdfwidget);
    
    //set up menu
    menu=ewl_context_menu_new();
    
    ewl_callback_append(menu, EWL_CALLBACK_KEY_DOWN, cb_menu_key_down, NULL);
    ewl_theme_data_str_set(EWL_WIDGET(menu),"/menu/group","ewl/menu/oi_menu");
    ewl_context_menu_attach(EWL_CONTEXT_MENU(menu), statbar);
    
    Ewl_Widget *temp=ewl_menu_new();
    ewl_container_child_append(EWL_CONTAINER(menu),temp);
    ewl_widget_name_set(temp,"menuitem1");
    ewl_button_label_set(EWL_BUTTON(temp),"1. Go to page...");
    ewl_widget_show(temp);

    goto_entry=ewl_entry_new();
    ewl_container_child_append(EWL_CONTAINER(temp),goto_entry);
    ewl_object_custom_w_set(EWL_OBJECT(goto_entry),50);
    ewl_callback_append(goto_entry, EWL_CALLBACK_KEY_DOWN, cb_goto_key_down, NULL);
    ewl_widget_show(goto_entry);
    
    
    temp=ewl_menu_item_new();
    ewl_widget_name_set(temp,"menuitem2");
    ewl_container_child_append(EWL_CONTAINER(menu),temp);
    ewl_button_label_set(EWL_BUTTON(temp),"2. Preferences...");
    ewl_widget_show(temp);

        
    
    
    ewl_main();
    save_settings(configfile);
    free(configfile);
    free_settings();
    return 0;
}

// vim:set ts=4 sw=4 et:
