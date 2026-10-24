#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#endif
#define SIDEBAR 284
#define ID_MODE 101
#define ID_REAL 102
#define ID_IMAG 103
#define ID_APPLY 104
#define ID_RESET 105
#define ID_ITER 106
#define ID_USE 107
#define ID_HIDE 108
#define ID_SHOW 109

typedef char GLchar;
#define GL_FN(ret, name, args) typedef ret (APIENTRY *name##Proc) args; static name##Proc name
GL_FN(GLuint, glCreateShader, (GLenum));
GL_FN(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *));
GL_FN(void, glCompileShader, (GLuint));
GL_FN(void, glGetShaderiv, (GLuint, GLenum, GLint *));
GL_FN(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
GL_FN(GLuint, glCreateProgram, (void));
GL_FN(void, glAttachShader, (GLuint, GLuint));
GL_FN(void, glLinkProgram, (GLuint));
GL_FN(void, glGetProgramiv, (GLuint, GLenum, GLint *));
GL_FN(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
GL_FN(void, glUseProgram, (GLuint));
GL_FN(void, glDeleteShader, (GLuint));
GL_FN(void, glDeleteProgram, (GLuint));
GL_FN(GLint, glGetUniformLocation, (GLuint, const GLchar *));
GL_FN(void, glUniform1i, (GLint, GLint));
GL_FN(void, glUniform1f, (GLint, GLfloat));
GL_FN(void, glUniform2f, (GLint, GLfloat, GLfloat));
GL_FN(void, glGenVertexArrays, (GLsizei, GLuint *));
GL_FN(void, glBindVertexArray, (GLuint));
GL_FN(void, glDeleteVertexArrays, (GLsizei, const GLuint *));

typedef HGLRC (WINAPI *CreateContextProc)(HDC, HGLRC, const int *);
static HWND window, panel, mode_box, real_edit, imag_edit, iter_edit, coord_label, zoom_label, show_button;
static HDC dc;
static HGLRC context;
static HFONT font, title_font;
static GLuint program, vao;
static GLint u_center, u_scale, u_size, u_c, u_mode, u_iter, u_point, u_selected;
static int width=1100, height=760, sidebar=1, julia=0, iterations=400;
static double center_x=-0.5, center_y=0, scale=3.0, cr=-0.745, ci=0.186;
static double point_x=0, point_y=0;
static int selected=0, dragging=0, moved=0, last_x, last_y, start_x, start_y;
static int smoke_test=0;

static const char *vertex_source =
    "#version 330 core\n"
    "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}\n";
static const char *fragment_source =
    "#version 330 core\n"
    "out vec4 color;uniform vec2 center,size,c,point;uniform float scale;"
    "uniform int mode,iterations,selected;\n"
    "void main(){vec2 p=center+(gl_FragCoord.xy-size*.5)*(scale/size.y);"
    "vec2 z=mode==0?vec2(0):p;vec2 k=mode==0?p:c;int n=0;float r=0;"
    "for(int i=0;i<4000;i++){if(i>=iterations)break;"
    "z=vec2(z.x*z.x-z.y*z.y,2*z.x*z.y)+k;r=dot(z,z);n=i+1;if(r>256.0)break;}"
    "vec3 rgb=vec3(.018,.025,.045);if(n<iterations){"
    "float t=float(n)+1.0-log2(log2(max(r,1.001)))*.5;"
    "rgb=.52+.48*cos(6.2831853*(vec3(.02,.18,.34)+t*.025));"
    "rgb*=.78+.22*(1.0-exp(-t*.08));}"
    "if(selected!=0){vec2 d=abs((p-point)*size.y/scale);"
    "if((d.x<.9&&d.y<8.0)||(d.y<.9&&d.x<8.0))rgb=vec3(1);"
    "else if((d.x<2.0&&d.y<9.0)||(d.y<2.0&&d.x<9.0))rgb=vec3(0);}"
    "color=vec4(rgb,1);}\n";

static void fail(const char *message){MessageBoxA(window,message,"Fractal viewer",MB_OK|MB_ICONERROR);ExitProcess(1);}
static int view_width(void){return width-(sidebar?SIDEBAR:0);}
static void invalidate(void){InvalidateRect(window,NULL,FALSE);}
static void coordinate(int x,int y,double *re,double *im){
    *re=center_x+(x-view_width()*.5)*scale/height;
    *im=center_y+(height*.5-y)*scale/height;
}
static void update_c(void){char b[80];snprintf(b,sizeof b,"%.12g",cr);SetWindowTextA(real_edit,b);snprintf(b,sizeof b,"%.12g",ci);SetWindowTextA(imag_edit,b);invalidate();}
static void update_point(void){char b[160];snprintf(b,sizeof b,"%.12g %c %.12gi",point_x,point_y<0?'-':'+',fabs(point_y));SetWindowTextA(coord_label,b);invalidate();}
static void reset_view(void){center_x=julia?0:-.5;center_y=0;scale=3;invalidate();}
static int parse_number(HWND edit,double *value){char b[128],*end;GetWindowTextA(edit,b,sizeof b);errno=0;double n=strtod(b,&end);if(end==b)return 0;while(isspace((unsigned char)*end))end++;if(*end||errno||!isfinite(n)||fabs(n)>1e6)return 0;*value=n;return 1;}
static void apply_values(void){
    double re,im,it;
    if(!parse_number(real_edit,&re)||!parse_number(imag_edit,&im)||!parse_number(iter_edit,&it)||it<32||it>4000||floor(it)!=it){MessageBoxA(window,"Enter finite C components between -1000000 and 1000000, and an integer iteration count from 32 to 4000.","Check settings",MB_OK|MB_ICONWARNING);return;}
    cr=re;ci=im;iterations=(int)it;update_c();SetFocus(window);
}
static HWND control(const char *cls,const char *text,DWORD style,int x,int y,int w,int h,int id){
    HWND c=CreateWindowExA(!lstrcmpA(cls,"EDIT")?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,panel,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
static WNDPROC panel_original;
static LRESULT CALLBACK panel_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_COMMAND)return SendMessage(window,msg,wp,lp);
    return CallWindowProc(panel_original,h,msg,wp,lp);
}
static void make_panel(void){
    panel=CreateWindowExA(0,"STATIC","",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,SIDEBAR,height,window,NULL,GetModuleHandle(NULL),NULL);
    panel_original=(WNDPROC)SetWindowLongPtr(panel,GWLP_WNDPROC,(LONG_PTR)panel_proc);
    HWND title=control("STATIC","FRACTAL EXPLORER",0,20,24,244,26,0);SendMessage(title,WM_SETFONT,(WPARAM)title_font,TRUE);
    control("STATIC","Set",0,20,70,244,20,0);
    mode_box=control("COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP,20,94,244,100,ID_MODE);
    SendMessageA(mode_box,CB_ADDSTRING,0,(LPARAM)"Mandelbrot");SendMessageA(mode_box,CB_ADDSTRING,0,(LPARAM)"Julia");SendMessage(mode_box,CB_SETCURSEL,0,0);
    control("STATIC","Julia parameter C",0,20,143,244,22,0);
    control("STATIC","Real",0,20,175,55,22,0);real_edit=control("EDIT","",ES_AUTOHSCROLL|WS_TABSTOP,77,171,187,27,ID_REAL);
    control("STATIC","Imaginary",0,20,211,60,22,0);imag_edit=control("EDIT","",ES_AUTOHSCROLL|WS_TABSTOP,77,207,187,27,ID_IMAG);
    control("STATIC","Iterations (32 - 4000)",0,20,252,244,22,0);iter_edit=control("EDIT","400",ES_NUMBER|WS_TABSTOP,20,278,244,27,ID_ITER);
    control("BUTTON","Apply settings",BS_PUSHBUTTON|WS_TABSTOP,20,319,244,32,ID_APPLY);
    control("STATIC","Selected coordinate",0,20,377,244,22,0);coord_label=control("STATIC","Click a point to inspect it",SS_LEFT,20,407,244,44,0);
    control("BUTTON","Use selected point as C",BS_PUSHBUTTON|WS_TABSTOP,20,461,244,32,ID_USE);
    control("BUTTON","Reset view",BS_PUSHBUTTON|WS_TABSTOP,20,509,117,32,ID_RESET);
    control("BUTTON","Hide sidebar",BS_PUSHBUTTON|WS_TABSTOP,147,509,117,32,ID_HIDE);
    zoom_label=control("STATIC","",0,20,557,244,22,0);
    control("STATIC","Drag to pan / wheel to zoom\r\nClick: inspect / right-click: set C\r\nArrows: adjust C by 0.005\r\nShift: finer C adjustment\r\nAlt + wheel: real C\r\nAlt + Shift + wheel: imaginary C\r\nTab: sidebar / Home: reset\r\nM / J: switch set",0,20,592,250,160,0);
    show_button=CreateWindowExA(0,"BUTTON","Settings  [Tab]",WS_CHILD|BS_PUSHBUTTON,12,12,128,30,window,(HMENU)ID_SHOW,GetModuleHandle(NULL),NULL);SendMessage(show_button,WM_SETFONT,(WPARAM)font,TRUE);
    update_c();
}
static void toggle_sidebar(void){sidebar=!sidebar;ShowWindow(panel,sidebar?SW_SHOW:SW_HIDE);ShowWindow(show_button,sidebar?SW_HIDE:SW_SHOW);invalidate();SetFocus(window);}
static void set_mode(int value){julia=value;SendMessage(mode_box,CB_SETCURSEL,julia,0);reset_view();SetFocus(window);}

static GLuint shader(GLenum kind,const char *source){GLuint s=glCreateShader(kind);glShaderSource(s,1,&source,NULL);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[2048];glGetShaderInfoLog(s,sizeof log,NULL,log);fail(log);}return s;}
static void init_gl(void){
    dc=GetDC(window);PIXELFORMATDESCRIPTOR p={0};p.nSize=sizeof p;p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;
    int format=ChoosePixelFormat(dc,&p);if(!format||!SetPixelFormat(dc,format,&p))fail("Cannot configure OpenGL pixel format.");
    HGLRC temp=wglCreateContext(dc);if(!temp||!wglMakeCurrent(dc,temp))fail("Cannot initialize OpenGL.");
    CreateContextProc create=(CreateContextProc)wglGetProcAddress("wglCreateContextAttribsARB");if(!create)fail("OpenGL 3.3 is required. Please install your graphics driver.");
    const int attrs[]={0x2091,3,0x2092,3,0x9126,1,0};context=create(dc,0,attrs);if(!context)fail("Your graphics driver could not create an OpenGL 3.3 core context.");
    wglMakeCurrent(NULL,NULL);wglDeleteContext(temp);if(!wglMakeCurrent(dc,context))fail("Cannot activate OpenGL context.");
#define LOAD(name) name=(name##Proc)wglGetProcAddress(#name);if(!name)fail("Missing OpenGL function: " #name)
    LOAD(glCreateShader);LOAD(glShaderSource);LOAD(glCompileShader);LOAD(glGetShaderiv);LOAD(glGetShaderInfoLog);LOAD(glCreateProgram);LOAD(glAttachShader);LOAD(glLinkProgram);LOAD(glGetProgramiv);LOAD(glGetProgramInfoLog);LOAD(glUseProgram);LOAD(glDeleteShader);LOAD(glDeleteProgram);LOAD(glGetUniformLocation);LOAD(glUniform1i);LOAD(glUniform1f);LOAD(glUniform2f);LOAD(glGenVertexArrays);LOAD(glBindVertexArray);LOAD(glDeleteVertexArrays);
#undef LOAD
    GLuint vs=shader(GL_VERTEX_SHADER,vertex_source),fs=shader(GL_FRAGMENT_SHADER,fragment_source);
    program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){char log[2048];glGetProgramInfoLog(program,sizeof log,NULL,log);fail(log);}glDeleteShader(vs);glDeleteShader(fs);glUseProgram(program);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    u_center=glGetUniformLocation(program,"center");u_scale=glGetUniformLocation(program,"scale");u_size=glGetUniformLocation(program,"size");u_c=glGetUniformLocation(program,"c");u_mode=glGetUniformLocation(program,"mode");u_iter=glGetUniformLocation(program,"iterations");u_point=glGetUniformLocation(program,"point");u_selected=glGetUniformLocation(program,"selected");
}
static void render(void){
    if(!program||height<=0||view_width()<=0)return;
    int w=view_width();glViewport(0,0,w,height);glUseProgram(program);
    glUniform2f(u_center,(float)center_x,(float)center_y);glUniform1f(u_scale,(float)scale);glUniform2f(u_size,(float)w,(float)height);glUniform2f(u_c,(float)cr,(float)ci);glUniform1i(u_mode,julia);glUniform1i(u_iter,iterations);glUniform2f(u_point,(float)point_x,(float)point_y);glUniform1i(u_selected,selected);glDrawArrays(GL_TRIANGLES,0,3);SwapBuffers(dc);
    char b[80];snprintf(b,sizeof b,"Zoom: %.1fx",3.0/scale);SetWindowTextA(zoom_label,b);
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_SIZE:width=LOWORD(lp);height=HIWORD(lp);if(panel)MoveWindow(panel,width-SIDEBAR,0,SIDEBAR,height,TRUE);invalidate();return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize.x=760;((MINMAXINFO*)lp)->ptMinTrackSize.y=820;return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);render();EndPaint(h,&ps);return 0;}
    case WM_COMMAND:switch(LOWORD(wp)){
        case ID_MODE:if(HIWORD(wp)==CBN_SELCHANGE)set_mode((int)SendMessage(mode_box,CB_GETCURSEL,0,0));break;
        case ID_APPLY:apply_values();break;
        case ID_RESET:reset_view();SetFocus(h);break;
        case ID_USE:if(selected){cr=point_x;ci=point_y;update_c();}SetFocus(h);break;
        case ID_HIDE:case ID_SHOW:toggle_sidebar();break;
    }return 0;
    case WM_LBUTTONDOWN:if(GET_X_LPARAM(lp)<view_width()){SetFocus(h);dragging=1;moved=0;start_x=last_x=GET_X_LPARAM(lp);start_y=last_y=GET_Y_LPARAM(lp);SetCapture(h);}return 0;
    case WM_MOUSEMOVE:if(dragging){int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);if(abs(x-start_x)>3||abs(y-start_y)>3)moved=1;if(moved){center_x-=(x-last_x)*scale/height;center_y+=(y-last_y)*scale/height;invalidate();}last_x=x;last_y=y;}return 0;
    case WM_LBUTTONUP:if(dragging){dragging=0;ReleaseCapture();if(!moved){coordinate(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),&point_x,&point_y);selected=1;update_point();}}return 0;
    case WM_CAPTURECHANGED:dragging=0;return 0;
    case WM_RBUTTONDOWN:if(GET_X_LPARAM(lp)<view_width()){SetFocus(h);coordinate(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),&point_x,&point_y);selected=1;cr=point_x;ci=point_y;update_point();update_c();}return 0;
    case WM_MOUSEWHEEL:{
        POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(h,&p);if(p.x<0||p.x>=view_width()||p.y<0||p.y>=height)return 0;
        double ticks=GET_WHEEL_DELTA_WPARAM(wp)/120.0;
        if(GetKeyState(VK_MENU)&0x8000){if(GetKeyState(VK_SHIFT)&0x8000)ci+=ticks*.001;else cr+=ticks*.001;update_c();}
        else{double x,y,nx,ny;coordinate(p.x,p.y,&x,&y);scale=fmax(0.00002,fmin(100.0,scale*pow(.82,ticks)));coordinate(p.x,p.y,&nx,&ny);center_x+=x-nx;center_y+=y-ny;invalidate();}return 0;}
    case WM_KEYDOWN:{double step=(GetKeyState(VK_SHIFT)&0x8000)?.0001:.005;switch(wp){
        case VK_TAB:toggle_sidebar();break;case VK_HOME:reset_view();break;
        case 'M':set_mode(0);break;case 'J':set_mode(1);break;
        case VK_LEFT:cr-=step;update_c();break;case VK_RIGHT:cr+=step;update_c();break;
        case VK_UP:ci+=step;update_c();break;case VK_DOWN:ci-=step;update_c();break;
    }return 0;}
    case WM_TIMER:if(smoke_test){render();GLenum error=glGetError();if(error!=GL_NO_ERROR)ExitProcess(2);PostMessage(h,WM_CLOSE,0,0);}return 0;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:if(context){wglMakeCurrent(dc,context);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);wglMakeCurrent(NULL,NULL);wglDeleteContext(context);}if(dc)ReleaseDC(h,dc);PostQuitMessage(0);return 0;
    }return DefWindowProcA(h,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE prev,LPSTR command,int show){
    (void)prev;smoke_test=strstr(command,"--smoke-test")!=NULL;SetProcessDPIAware();
    font=CreateFontA(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    title_font=CreateFontA(-18,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    WNDCLASSA wc={0};wc.style=CS_OWNDC;wc.lpfnWndProc=proc;wc.hInstance=instance;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.lpszClassName="FractalExplorer";if(!RegisterClassA(&wc))fail("Cannot register window class.");
    window=CreateWindowExA(0,wc.lpszClassName,"Fractal Explorer",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1160,860,NULL,NULL,instance,NULL);if(!window)fail("Cannot create window.");
    RECT r;GetClientRect(window,&r);width=r.right;height=r.bottom;make_panel();MoveWindow(panel,width-SIDEBAR,0,SIDEBAR,height,TRUE);init_gl();ShowWindow(window,show);UpdateWindow(window);if(smoke_test)SetTimer(window,1,250,NULL);
    MSG msg;while(GetMessage(&msg,NULL,0,0)>0){if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN&&(GetFocus()==real_edit||GetFocus()==imag_edit||GetFocus()==iter_edit)){apply_values();continue;}if(msg.message==WM_KEYDOWN&&msg.wParam==VK_TAB){toggle_sidebar();continue;}TranslateMessage(&msg);DispatchMessage(&msg);}
    DeleteObject(font);DeleteObject(title_font);return (int)msg.wParam;
}
