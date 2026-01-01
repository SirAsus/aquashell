#include "inc.h"

#define BORDER_WIDTH 2
#define TITLEBAR_HEIGHT 22
#define BUTTON_SIZE 14
#define BUTTON_SPACING 6
#define MIN_WIDTH 100
#define MIN_HEIGHT 80
#define RESIZE_HANDLE_SIZE 8

typedef struct {
    Window window;
    Window frame;
    int x, y;
    int width, height;
    int frame_x, frame_y;
    int orig_x, orig_y;
    int orig_width, orig_height;
    Bool mapped;
    Bool decorated;
    char *title;
    int ignore_unmap;
    int is_active;
    int is_maximized;
    int is_fullscreen;
    int is_dock;
    int dock_side;
    int strut_size;
} Client;

typedef struct {
    Display *display;
    int screen;
    Window root;
    Visual *visual;
    int depth;
    int screen_width;
    int screen_height;
    int workarea_x, workarea_y, workarea_width, workarea_height;
    
    Pixmap desktop_pixmap;
    int desktop_drawn;
    
    Atom wm_protocols;
    Atom wm_delete_window;
    Atom wm_take_focus;
    Atom net_wm_state;
    Atom net_wm_state_maximized_vert;
    Atom net_wm_state_maximized_horz;
    Atom net_wm_state_fullscreen;
    Atom net_active_window;
    Atom wm_transient_for;
    Atom utf8_string;
    Atom net_wm_window_type;
    Atom net_wm_window_type_dock;
    Atom net_wm_window_type_desktop;
    Atom net_wm_strut;
    Atom net_wm_strut_partial;
    Atom net_wm_state_above;
    Atom net_wm_state_stays_on_top;
    Atom net_wm_state_below;
    
    Client **clients;
    int num_clients;
    Client *active_client;
    Client *moving_client;
    Client *grabbed_client;
    
    Cursor cursor_normal;
    Cursor cursor_resize;
    Cursor cursor_move;
    Cursor cursor_resize_hor;
    Cursor cursor_resize_ver;
    Cursor cursor_resize_diag1;
    Cursor cursor_resize_diag2;
    
    int drag_start_x;
    int drag_start_y;
    int is_moving;
    int is_resizing;
    int resize_edge;
    int move_start_x;
    int move_start_y;
    unsigned int drag_button;
    
    int mouse_grabbed;
    Window grab_window;
    
    GC frame_gc;
    GC button_gc;
    GC border_gc;
    GC desktop_gc;
} AquaWm;

Client* find_client_by_window(AquaWm *wm, Window win);
Client* find_client_by_frame(AquaWm *wm, Window frame);
void remove_client(AquaWm *wm, Client *client);
void send_configure_notify(AquaWm *wm, Client *client);
void draw_frame(AquaWm *wm, Client *client);
void update_cursor(AquaWm *wm, Window window, int x, int y);
int get_resize_edge(Client *client, int x, int y);
void center_window(AquaWm *wm, Client *client);
void draw_window(AquaWm *wm, Client *client);
char* get_window_title(AquaWm *wm, Window window);
void toggle_maximize(AquaWm *wm, Client *client);
void toggle_fullscreen(AquaWm *wm, Client *client);
void minimize_window(AquaWm *wm, Client *client);
int is_over_button(int x, int y, int button_x, int button_y);
int is_dock_or_desktop(AquaWm *wm, Window window);
void apply_window_state(AquaWm *wm, Client *client);
void update_dock_struts(AquaWm *wm);
void keep_docks_on_top(AquaWm *wm);
void enforce_dock_above(AquaWm *wm, Client *dock);
int is_likely_dock(AquaWm *wm, Window window);
void set_window_above(AquaWm *wm, Window window);
void calculate_workarea(AquaWm *wm);
void draw_button(AquaWm *wm, Window win, int x, int y, int is_hover, int is_pressed, int type, int is_active);
void create_desktop_pattern(AquaWm *wm);
void draw_rounded_rect(Display *dpy, Window win, GC gc, int x, int y, int w, int h, int radius);

AquaWm* aquawm_init(void) {
    AquaWm *wm = calloc(1, sizeof(AquaWm));
    if (!wm) return NULL;
    
    wm->display = XOpenDisplay(NULL);
    if (!wm->display) {
        free(wm);
        return NULL;
    }
    
    wm->screen = DefaultScreen(wm->display);
    wm->root = RootWindow(wm->display, wm->screen);
    wm->visual = DefaultVisual(wm->display, wm->screen);
    wm->depth = DefaultDepth(wm->display, wm->screen);
    wm->screen_width = DisplayWidth(wm->display, wm->screen);
    wm->screen_height = DisplayHeight(wm->display, wm->screen);
    wm->workarea_x = 0;
    wm->workarea_y = 0;
    wm->workarea_width = wm->screen_width;
    wm->workarea_height = wm->screen_height;
    
    wm->frame_gc = XCreateGC(wm->display, wm->root, 0, NULL);
    wm->button_gc = XCreateGC(wm->display, wm->root, 0, NULL);
    wm->border_gc = XCreateGC(wm->display, wm->root, 0, NULL);
    wm->desktop_gc = XCreateGC(wm->display, wm->root, 0, NULL);
    
    XSetLineAttributes(wm->display, wm->border_gc, BORDER_WIDTH, LineSolid, CapButt, JoinMiter);
    
    create_desktop_pattern(wm);
    
    wm->wm_protocols = XInternAtom(wm->display, "WM_PROTOCOLS", False);
    wm->wm_delete_window = XInternAtom(wm->display, "WM_DELETE_WINDOW", False);
    wm->wm_take_focus = XInternAtom(wm->display, "WM_TAKE_FOCUS", False);
    wm->net_wm_state = XInternAtom(wm->display, "_NET_WM_STATE", False);
    wm->net_wm_state_maximized_vert = XInternAtom(wm->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    wm->net_wm_state_maximized_horz = XInternAtom(wm->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    wm->net_wm_state_fullscreen = XInternAtom(wm->display, "_NET_WM_STATE_FULLSCREEN", False);
    wm->net_active_window = XInternAtom(wm->display, "_NET_ACTIVE_WINDOW", False);
    wm->wm_transient_for = XInternAtom(wm->display, "WM_TRANSIENT_FOR", False);
    wm->utf8_string = XInternAtom(wm->display, "UTF8_STRING", False);
    wm->net_wm_window_type = XInternAtom(wm->display, "_NET_WM_WINDOW_TYPE", False);
    wm->net_wm_window_type_dock = XInternAtom(wm->display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    wm->net_wm_window_type_desktop = XInternAtom(wm->display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    wm->net_wm_strut = XInternAtom(wm->display, "_NET_WM_STRUT", False);
    wm->net_wm_strut_partial = XInternAtom(wm->display, "_NET_WM_STRUT_PARTIAL", False);
    wm->net_wm_state_above = XInternAtom(wm->display, "_NET_WM_STATE_ABOVE", False);
    wm->net_wm_state_stays_on_top = XInternAtom(wm->display, "_NET_WM_STATE_STAYS_ON_TOP", False);
    wm->net_wm_state_below = XInternAtom(wm->display, "_NET_WM_STATE_BELOW", False);
    
    wm->cursor_normal = XCreateFontCursor(wm->display, XC_left_ptr);
    wm->cursor_resize = XCreateFontCursor(wm->display, XC_bottom_right_corner);
    wm->cursor_move = XCreateFontCursor(wm->display, XC_fleur);
    wm->cursor_resize_hor = XCreateFontCursor(wm->display, XC_sb_h_double_arrow);
    wm->cursor_resize_ver = XCreateFontCursor(wm->display, XC_sb_v_double_arrow);
    wm->cursor_resize_diag1 = XCreateFontCursor(wm->display, XC_bottom_right_corner);
    wm->cursor_resize_diag2 = XCreateFontCursor(wm->display, XC_bottom_left_corner);
    
    XDefineCursor(wm->display, wm->root, wm->cursor_normal);
    
    wm->clients = NULL;
    wm->num_clients = 0;
    wm->active_client = NULL;
    wm->moving_client = NULL;
    wm->grabbed_client = NULL;
    wm->is_moving = 0;
    wm->is_resizing = 0;
    wm->resize_edge = 0;
    wm->drag_button = 0;
    wm->mouse_grabbed = 0;
    wm->grab_window = None;
    
    XSetWindowAttributes attrs;
    unsigned long attr_mask = CWEventMask;
    
    attrs.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                       ButtonPressMask | ButtonReleaseMask |
                       KeyPressMask | KeyReleaseMask |
                       EnterWindowMask | LeaveWindowMask |
                       PointerMotionMask |
                       FocusChangeMask;
    
    if (wm->desktop_pixmap != None) {
        attrs.background_pixmap = wm->desktop_pixmap;
        attr_mask |= CWBackPixmap;
    } else {
        attrs.background_pixel = 0x4169E1;
        attr_mask |= CWBackPixel;
    }
    
    XChangeWindowAttributes(wm->display, wm->root, attr_mask, &attrs);
    
    XClearWindow(wm->display, wm->root);
    XFlush(wm->display);
    
    XGrabKey(wm->display, XKeysymToKeycode(wm->display, XK_F4), 
             Mod4Mask, wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->display, XKeysymToKeycode(wm->display, XK_Tab), 
             Mod4Mask, wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->display, XKeysymToKeycode(wm->display, XK_F11), 
             0, wm->root, True, GrabModeAsync, GrabModeAsync);
    
    printf("welcome to AquaWM!\n");
    printf("desktop size: %dx%d\n", wm->screen_width, wm->screen_height);
    return wm;
}

void create_desktop_pattern(AquaWm *wm) {
    if (wm->desktop_pixmap != None) {
        XFreePixmap(wm->display, wm->desktop_pixmap);
    }
    
    printf("creating desktop pattern\n");
    
    wm->desktop_pixmap = XCreatePixmap(wm->display, wm->root, 
                                      wm->screen_width, wm->screen_height, wm->depth);
    
    if (wm->desktop_pixmap == None) {
        fprintf(stderr, "failed to create desktop pixmap!!\n");
        return;
    }
    
    for (int y = 0; y < wm->screen_height; y++) {
        int r, g, b;
        float t = (float)y / wm->screen_height;
        
        r = 180 * (1 - t) + 40 * t;
        g = 230 * (1 - t) + 100 * t;
        b = 255 * (1 - t) + 150 * t;
        
        int color = (r << 16) | (g << 8) | b;
        
        XSetForeground(wm->display, wm->desktop_gc, color);
        XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc, 
                 0, y, wm->screen_width, y);
    }
    
    int grid_size = 100;
    
    for (int y = 0; y < wm->screen_height; y += grid_size) {
        for (int x = 0; x < wm->screen_width; x += grid_size) {
            XSetForeground(wm->display, wm->desktop_gc, 0x6495ED);
            XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                     x + grid_size/2, y,
                     x + grid_size, y + grid_size/2);
            XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                     x + grid_size, y + grid_size/2,
                     x + grid_size/2, y + grid_size);
            XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                     x + grid_size/2, y + grid_size,
                     x, y + grid_size/2);
            XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                     x, y + grid_size/2,
                     x + grid_size/2, y);
            
            XSetForeground(wm->display, wm->desktop_gc, 0x4169E1);
            XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                     x + grid_size/2 - 10, y + grid_size/2,
                     x + grid_size/2 + 10, y + grid_size/2);
            XDrawLine(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                     x + grid_size/2, y + grid_size/2 - 10,
                     x + grid_size/2, y + grid_size/2 + 10);
        }
    }
    
    XFontStruct* font = XLoadQueryFont(wm->display, "fixed");
    if (font) {
        XSetFont(wm->display, wm->desktop_gc, font->fid);
        XSetForeground(wm->display, wm->desktop_gc, 0x3A6F9F);
        XDrawString(wm->display, wm->desktop_pixmap, wm->desktop_gc,
                   wm->screen_width - 120, wm->screen_height - 20,
                   "AquaWM", 6);
        XFreeFont(wm->display, font);
    }
    
    wm->desktop_drawn = 1;
    printf("Desktop pattern created successfully\n");
}

void calculate_workarea(AquaWm *wm) {
    wm->workarea_x = 0;
    wm->workarea_y = 0;
    wm->workarea_width = wm->screen_width;
    wm->workarea_height = wm->screen_height;
    
    for (int i = 0; i < wm->num_clients; i++) {
        if (wm->clients[i]->is_dock && wm->clients[i]->mapped) {
            Client *dock = wm->clients[i];
            
            if (dock->y < wm->screen_height / 4) {
                wm->workarea_y += dock->height;
                wm->workarea_height -= dock->height;
            } else if (dock->y > wm->screen_height * 3 / 4) {
                wm->workarea_height -= dock->height;
            } else if (dock->x < wm->screen_width / 4) {
                wm->workarea_x += dock->width;
                wm->workarea_width -= dock->width;
            } else if (dock->x > wm->screen_width * 3 / 4) {
                wm->workarea_width -= dock->width;
            }
        }
    }
    
    if (wm->workarea_width < MIN_WIDTH * 2) wm->workarea_width = MIN_WIDTH * 2;
    if (wm->workarea_height < MIN_HEIGHT * 2) wm->workarea_height = MIN_HEIGHT * 2;
}

int is_likely_dock(AquaWm *wm, Window window) {
    XTextProperty text_prop;
    char *title = NULL;
    int likely_dock = 0;
    
    if (XGetWMName(wm->display, window, &text_prop)) {
        if (text_prop.value && text_prop.nitems > 0) {
            title = strdup((char*)text_prop.value);
        }
        XFree(text_prop.value);
    }
    
    if (title) {
        const char *dock_names[] = {
            "panel", "Panel", "xfce4-panel", "mate-panel", 
            "gnome-panel", "tint2", "polybar", "aquabar", "aquapanel",
            "xmobar", "lemonbar", "vala-panel", "plank", "dock"
        };
        
        for (unsigned int i = 0; i < sizeof(dock_names)/sizeof(dock_names[0]); i++) {
            if (strstr(title, dock_names[i]) != NULL) {
                likely_dock = 1;
                break;
            }
        }
        
        free(title);
    }
    
    if (!likely_dock) {
        XWindowAttributes attr;
        if (XGetWindowAttributes(wm->display, window, &attr)) {
            if ((attr.height < 100 && (attr.y < 10 || attr.y > wm->screen_height - attr.height - 10)) ||
                (attr.width < 100 && (attr.x < 10 || attr.x > wm->screen_width - attr.width - 10))) {
                likely_dock = 1;
            }
        }
    }
    
    return likely_dock;
}

int is_dock_or_desktop(AquaWm *wm, Window window) {
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    int result = 0;
    
    if (XGetWindowProperty(wm->display, window, wm->net_wm_window_type,
                          0, 10, False, XA_ATOM, &type, &format,
                          &nitems, &bytes_after, &data) == Success && data) {
        Atom *atoms = (Atom*)data;
        for (unsigned long i = 0; i < nitems; i++) {
            if (atoms[i] == wm->net_wm_window_type_dock ||
                atoms[i] == wm->net_wm_window_type_desktop) {
                result = 1;
                break;
            }
        }
        XFree(data);
    }
    
    if (!result && XGetWindowProperty(wm->display, window, wm->net_wm_state,
                              0, 10, False, XA_ATOM, &type, &format,
                              &nitems, &bytes_after, &data) == Success && data) {
        Atom *atoms = (Atom*)data;
        for (unsigned long i = 0; i < nitems; i++) {
            if (atoms[i] == wm->net_wm_state_above ||
                atoms[i] == wm->net_wm_state_stays_on_top) {
                result = 1;
                break;
            }
        }
        XFree(data);
    }
    
    if (!result) {
        result = is_likely_dock(wm, window);
    }
    
    return result;
}

void set_window_above(AquaWm *wm, Window window) {
    Atom atoms[] = {wm->net_wm_state_above, wm->net_wm_state_stays_on_top};
    XChangeProperty(wm->display, window, wm->net_wm_state,
                   XA_ATOM, 32, PropModeReplace, 
                   (unsigned char*)atoms, 2);
}

void enforce_dock_above(AquaWm *wm, Client *dock) {
    if (!dock || !dock->is_dock) return;
    
    set_window_above(wm, dock->window);
    XRaiseWindow(wm->display, dock->window);
    
    Atom below_atoms[] = {wm->net_wm_state_below};
    XChangeProperty(wm->display, dock->window, wm->net_wm_state,
                   XA_ATOM, 32, PropModeAppend, 
                   (unsigned char*)below_atoms, 1);
}

void update_dock_struts(AquaWm *wm) {
    for (int i = 0; i < wm->num_clients; i++) {
        if (wm->clients[i]->is_dock && wm->clients[i]->mapped) {
            Client *dock = wm->clients[i];
            
            Window root;
            int x, y;
            unsigned int width, height, border, depth;
            if (XGetGeometry(wm->display, dock->window, &root, &x, &y, 
                           &width, &height, &border, &depth)) {
                
                dock->x = x;
                dock->y = y;
                dock->width = (int)width;
                dock->height = (int)height;
                
                if (y < wm->screen_height / 2) {
                    dock->dock_side = 0;
                    dock->strut_size = (int)height;
                } else if (y + (int)height > wm->screen_height * 3 / 4) {
                    dock->dock_side = 1;
                    dock->strut_size = (int)height;
                } else if (x < wm->screen_width / 2) {
                    dock->dock_side = 2;
                    dock->strut_size = (int)width;
                } else {
                    dock->dock_side = 3;
                    dock->strut_size = (int)width;
                }
                
                unsigned long strut[12] = {0};
                
                switch (dock->dock_side) {
                    case 0:
                        strut[2] = (unsigned long)height;
                        strut[6] = (unsigned long)x;
                        strut[7] = (unsigned long)(x + (int)width - 1);
                        break;
                    case 1:
                        strut[3] = (unsigned long)height;
                        strut[8] = (unsigned long)x;
                        strut[9] = (unsigned long)(x + (int)width - 1);
                        break;
                    case 2:
                        strut[0] = (unsigned long)width;
                        strut[4] = (unsigned long)y;
                        strut[5] = (unsigned long)(y + (int)height - 1);
                        break;
                    case 3:
                        strut[1] = (unsigned long)width;
                        strut[10] = (unsigned long)y;
                        strut[11] = (unsigned long)(y + (int)height - 1);
                        break;
                }
                
                XChangeProperty(wm->display, dock->window, wm->net_wm_strut,
                               XA_CARDINAL, 32, PropModeReplace, 
                               (unsigned char*)strut, 4);
                
                XChangeProperty(wm->display, dock->window, wm->net_wm_strut_partial,
                               XA_CARDINAL, 32, PropModeReplace, 
                               (unsigned char*)strut, 12);
            }
        }
    }
    
    calculate_workarea(wm);
}

void keep_docks_on_top(AquaWm *wm) {
    for (int i = 0; i < wm->num_clients; i++) {
        if (wm->clients[i]->is_dock && wm->clients[i]->mapped) {
            XRaiseWindow(wm->display, wm->clients[i]->window);
        }
    }
}

void apply_window_state(AquaWm *wm, Client *client) {
    if (client->is_fullscreen) {
        if (client->decorated) {
            XUnmapWindow(wm->display, client->frame);
        }
        XMoveResizeWindow(wm->display, client->window,
                         0, 0,
                         wm->screen_width, wm->screen_height);
    } else if (client->is_maximized) {
        if (client->decorated) {
            XMoveResizeWindow(wm->display, client->frame,
                             wm->workarea_x,
                             wm->workarea_y,
                             wm->workarea_width,
                             wm->workarea_height);
            XResizeWindow(wm->display, client->window,
                         wm->workarea_width - 2 * BORDER_WIDTH,
                         wm->workarea_height - TITLEBAR_HEIGHT - BORDER_WIDTH);
        } else {
            XMoveResizeWindow(wm->display, client->window,
                             wm->workarea_x,
                             wm->workarea_y,
                             wm->workarea_width,
                             wm->workarea_height);
        }
    } else if (client->decorated) {
        XMoveResizeWindow(wm->display, client->frame,
                         client->frame_x, client->frame_y,
                         client->width + 2 * BORDER_WIDTH,
                         client->height + TITLEBAR_HEIGHT + BORDER_WIDTH);
        XResizeWindow(wm->display, client->window, client->width, client->height);
    }
}

void center_window(AquaWm *wm, Client *client) {
    if (!client || !wm || client->is_dock) return;
    
    int frame_width = client->width + 2 * BORDER_WIDTH;
    int frame_height = client->height + TITLEBAR_HEIGHT + BORDER_WIDTH;
    
    client->frame_x = wm->workarea_x + (wm->workarea_width - frame_width) / 2;
    client->frame_y = wm->workarea_y + (wm->workarea_height - frame_height) / 2;
    
    if (client->frame_x < wm->workarea_x) client->frame_x = wm->workarea_x;
    if (client->frame_y < wm->workarea_y) client->frame_y = wm->workarea_y;
    if (client->frame_x + frame_width > wm->workarea_x + wm->workarea_width)
        client->frame_x = wm->workarea_x + wm->workarea_width - frame_width;
    if (client->frame_y + frame_height > wm->workarea_y + wm->workarea_height)
        client->frame_y = wm->workarea_y + wm->workarea_height - frame_height;
    
    client->x = client->frame_x + BORDER_WIDTH;
    client->y = client->frame_y + TITLEBAR_HEIGHT;
    
    client->orig_x = client->x;
    client->orig_y = client->y;
    client->orig_width = client->width;
    client->orig_height = client->height;
}

void toggle_maximize(AquaWm *wm, Client *client) {
    if (!client || client->is_fullscreen || client->is_dock) return;
    
    if (client->is_maximized) {
        client->x = client->orig_x;
        client->y = client->orig_y;
        client->width = client->orig_width;
        client->height = client->orig_height;
        client->frame_x = client->x - BORDER_WIDTH;
        client->frame_y = client->y - TITLEBAR_HEIGHT;
        client->is_maximized = 0;
    } else {
        client->orig_x = client->x;
        client->orig_y = client->y;
        client->orig_width = client->width;
        client->orig_height = client->height;
        
        client->frame_x = wm->workarea_x;
        client->frame_y = wm->workarea_y;
        client->x = client->frame_x + BORDER_WIDTH;
        client->y = client->frame_y + TITLEBAR_HEIGHT;
        client->width = wm->workarea_width - 2 * BORDER_WIDTH;
        client->height = wm->workarea_height - TITLEBAR_HEIGHT - BORDER_WIDTH;
        client->is_maximized = 1;
    }
    
    apply_window_state(wm, client);
    send_configure_notify(wm, client);
    if (client->decorated && !client->is_fullscreen) {
        draw_frame(wm, client);
    }
}

void toggle_fullscreen(AquaWm *wm, Client *client) {
    if (!client || client->is_dock) return;
    
    if (client->is_fullscreen) {
        client->is_fullscreen = 0;
        client->is_maximized = 0;
        client->x = client->orig_x;
        client->y = client->orig_y;
        client->width = client->orig_width;
        client->height = client->orig_height;
        client->frame_x = client->x - BORDER_WIDTH;
        client->frame_y = client->y - TITLEBAR_HEIGHT;
        
        if (client->decorated) {
            XMapWindow(wm->display, client->frame);
        }
    } else {
        client->orig_x = client->x;
        client->orig_y = client->y;
        client->orig_width = client->width;
        client->orig_height = client->height;
        
        client->is_fullscreen = 1;
        client->is_maximized = 0;
        client->x = 0;
        client->y = 0;
        client->width = wm->screen_width;
        client->height = wm->screen_height;
    }
    
    apply_window_state(wm, client);
    send_configure_notify(wm, client);
    if (client->decorated && !client->is_fullscreen) {
        draw_frame(wm, client);
    }
}

void minimize_window(AquaWm *wm, Client *client) {
    if (!client || client->is_dock) return;
    
    if (client->decorated) {
        XUnmapWindow(wm->display, client->frame);
    }
    XUnmapWindow(wm->display, client->window);
}

int is_over_button(int x, int y, int button_x, int button_y) {
    return (x >= button_x && x <= button_x + BUTTON_SIZE &&
            y >= button_y && y <= button_y + BUTTON_SIZE);
}

void update_cursor(AquaWm *wm, Window window, int x, int y) {
    Client *client = find_client_by_frame(wm, window);
    if (!client) return;
    
    int frame_width = client->width + 2 * BORDER_WIDTH;
    int close_x = BORDER_WIDTH + BUTTON_SPACING;
    int close_y = BORDER_WIDTH + (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
    int maximize_x = frame_width - BORDER_WIDTH - BUTTON_SPACING - 2*BUTTON_SIZE - 2*BUTTON_SPACING;
    int maximize_y = close_y;
    int minimize_x = frame_width - BORDER_WIDTH - BUTTON_SPACING - BUTTON_SIZE - BUTTON_SPACING;
    int minimize_y = close_y;
    
    if (is_over_button(x, y, close_x, close_y) ||
        is_over_button(x, y, maximize_x, maximize_y) ||
        is_over_button(x, y, minimize_x, minimize_y)) {
        XDefineCursor(wm->display, window, wm->cursor_normal);
        return;
    }
    
    Cursor new_cursor = wm->cursor_normal;
    
    if (y < TITLEBAR_HEIGHT) {
        new_cursor = wm->cursor_move;
    } else {
        int edge = get_resize_edge(client, x, y);
        switch (edge) {
            case 1:
            case 2:
                new_cursor = wm->cursor_resize_hor;
                break;
            case 4:
            case 8:
                new_cursor = wm->cursor_resize_ver;
                break;
            case 5:
                new_cursor = wm->cursor_resize_diag2;
                break;
            case 6:
            case 9:
            case 10:
                new_cursor = wm->cursor_resize_diag1;
                break;
            default:
                new_cursor = wm->cursor_normal;
                break;
        }
    }
    
    XDefineCursor(wm->display, window, new_cursor);
}

int get_resize_edge(Client *client, int x, int y) {
    int edge = 0;
    int frame_width = client->width + 2 * BORDER_WIDTH;
    int frame_height = client->height + TITLEBAR_HEIGHT + BORDER_WIDTH;
    
    if (x < RESIZE_HANDLE_SIZE) edge |= 1;
    if (x > frame_width - RESIZE_HANDLE_SIZE) edge |= 2;
    if (y > TITLEBAR_HEIGHT && y < TITLEBAR_HEIGHT + RESIZE_HANDLE_SIZE) edge |= 4;
    if (y > frame_height - RESIZE_HANDLE_SIZE) edge |= 8;
    
    return edge;
}

void create_frame(AquaWm *wm, Client *client) {
    XWindowAttributes attr;
    if (!XGetWindowAttributes(wm->display, client->window, &attr)) {
        free(client);
        return;
    }
    
    if (attr.override_redirect) {
        XMapWindow(wm->display, client->window);
        free(client);
        return;
    }
    
    int dock_type = is_dock_or_desktop(wm, client->window);
    if (dock_type) {
        client->decorated = 0;
        client->frame = client->window;
        client->is_dock = 1;
        client->dock_side = -1;
        client->strut_size = 0;
        
        enforce_dock_above(wm, client);
        
        XMapWindow(wm->display, client->window);
        XRaiseWindow(wm->display, client->window);
        
        XWindowAttributes win_attr;
        if (XGetWindowAttributes(wm->display, client->window, &win_attr)) {
            client->x = win_attr.x;
            client->y = win_attr.y;
            client->width = win_attr.width;
            client->height = win_attr.height;
            client->mapped = True;
        }
        
        wm->clients = realloc(wm->clients, (wm->num_clients + 1) * sizeof(Client*));
        wm->clients[wm->num_clients] = client;
        wm->num_clients++;
        
        update_dock_struts(wm);
        keep_docks_on_top(wm);
        return;
    }
    
    client->decorated = 1;
    client->is_dock = 0;
    client->ignore_unmap = 0;
    client->is_active = 0;
    client->is_maximized = 0;
    client->is_fullscreen = 0;
    client->title = NULL;
    
    XWindowAttributes win_attr;
    XGetWindowAttributes(wm->display, client->window, &win_attr);
    
    client->width = win_attr.width > MIN_WIDTH ? win_attr.width : MIN_WIDTH;
    client->height = win_attr.height > MIN_HEIGHT ? win_attr.height : MIN_HEIGHT;
    
    center_window(wm, client);
    
    client->frame = XCreateSimpleWindow(wm->display, wm->root,
                                       client->frame_x,
                                       client->frame_y,
                                       client->width + 2 * BORDER_WIDTH,
                                       client->height + TITLEBAR_HEIGHT + BORDER_WIDTH,
                                       BORDER_WIDTH,
                                       0x666666,
                                       0xF0F0F0);
    
    XSelectInput(wm->display, client->frame, 
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 ButtonMotionMask | EnterWindowMask | PointerMotionMask |
                 LeaveWindowMask | FocusChangeMask);
    
    Atom protocols[] = {wm->wm_delete_window, wm->wm_take_focus};
    XSetWMProtocols(wm->display, client->frame, protocols, 2);
    
    XReparentWindow(wm->display, client->window, client->frame,
                   BORDER_WIDTH, TITLEBAR_HEIGHT);
    
    XSelectInput(wm->display, client->window, StructureNotifyMask);
    
    client->mapped = False;
    
    wm->clients = realloc(wm->clients, (wm->num_clients + 1) * sizeof(Client*));
    wm->clients[wm->num_clients] = client;
    wm->num_clients++;
    
    XMapWindow(wm->display, client->frame);
    XDefineCursor(wm->display, client->frame, wm->cursor_normal);
}

void draw_button(AquaWm *wm, Window win, int x, int y, int is_hover, int is_pressed, int type, int is_active) {
    Display *dpy = wm->display;
    GC gc = wm->button_gc;
    
    int bg_color_top, bg_color_bottom, border_light, border_dark;
    
    if (is_active) {
        if (is_pressed) {
            bg_color_top = 0x2A3D88;
            bg_color_bottom = 0x1A2D78;
            border_light = 0x0A1D68;
            border_dark = 0x4A5DA8;
        } else if (is_hover) {
            bg_color_top = 0x4B6DD8;
            bg_color_bottom = 0x3B5DC8;
            border_light = 0x5B7DE8;
            border_dark = 0x2B4DA8;
        } else {
            bg_color_top = 0x8090D0;
            bg_color_bottom = 0x6070B0;
            border_light = 0xA0B0E0;
            border_dark = 0x506090;
        }
    } else {
        if (is_pressed) {
            bg_color_top = 0x666666;
            bg_color_bottom = 0x444444;
            border_light = 0x222222;
            border_dark = 0x888888;
        } else if (is_hover) {
            bg_color_top = 0xAAAAAA;
            bg_color_bottom = 0x888888;
            border_light = 0xBBBBBB;
            border_dark = 0x666666;
        } else {
            bg_color_top = 0xCCCCCC;
            bg_color_bottom = 0xAAAAAA;
            border_light = 0xDDDDDD;
            border_dark = 0x888888;
        }
    }
    
    for (int i = 0; i < BUTTON_SIZE; i++) {
        float t = (float)i / (BUTTON_SIZE - 1);
        int r = ((bg_color_top >> 16) & 0xFF) * (1 - t) + ((bg_color_bottom >> 16) & 0xFF) * t;
        int g = ((bg_color_top >> 8) & 0xFF) * (1 - t) + ((bg_color_bottom >> 8) & 0xFF) * t;
        int b = (bg_color_top & 0xFF) * (1 - t) + (bg_color_bottom & 0xFF) * t;
        int color = (r << 16) | (g << 8) | b;
        
        XSetForeground(dpy, gc, color);
        XDrawLine(dpy, win, gc, x, y + i, x + BUTTON_SIZE - 1, y + i);
    }
    
    XSetForeground(dpy, gc, border_light);
    XDrawLine(dpy, win, gc, x, y, x + BUTTON_SIZE - 1, y);
    XDrawLine(dpy, win, gc, x, y, x, y + BUTTON_SIZE - 1);
    
    XSetForeground(dpy, gc, border_dark);
    XDrawLine(dpy, win, gc, x, y + BUTTON_SIZE - 1, x + BUTTON_SIZE - 1, y + BUTTON_SIZE - 1);
    XDrawLine(dpy, win, gc, x + BUTTON_SIZE - 1, y, x + BUTTON_SIZE - 1, y + BUTTON_SIZE - 1);
    
    XSetForeground(dpy, gc, is_hover ? 0xFFFFFF : (is_pressed ? 0xD0D0D0 : 0x404040));
    
    if (type == 0) {
        XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
        XDrawLine(dpy, win, gc, x + 4, y + 4, x + BUTTON_SIZE - 4, y + BUTTON_SIZE - 4);
        XDrawLine(dpy, win, gc, x + BUTTON_SIZE - 4, y + 4, x + 4, y + BUTTON_SIZE - 4);
        XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
    } else if (type == 1) {
        XSetLineAttributes(dpy, gc, 2, LineSolid, CapButt, JoinMiter);
        XDrawRectangle(dpy, win, gc, x + 3, y + 3, BUTTON_SIZE - 6, BUTTON_SIZE - 6);
        XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
    } else if (type == 2) {
        XSetLineAttributes(dpy, gc, 2, LineSolid, CapButt, JoinMiter);
        XDrawLine(dpy, win, gc, x + 4, y + BUTTON_SIZE - 6, x + BUTTON_SIZE - 4, y + BUTTON_SIZE - 6);
        XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
    }
}

void draw_window(AquaWm *wm, Client *client) {
    if (!client->decorated || client->is_fullscreen || client->is_dock) return;
    
    Display *dpy = wm->display;
    Window win = client->frame;
    int frame_width = client->width + 2 * BORDER_WIDTH;
    int frame_height = client->height + TITLEBAR_HEIGHT + BORDER_WIDTH;
    
    GC gc = wm->frame_gc;
    
    int gradient_steps = 16;
    for (int i = 0; i < gradient_steps; i++) {
        float t = (float)i / (gradient_steps - 1);
        int r, g, b;
        int x1 = i * frame_width / gradient_steps;
        int x2 = (i + 1) * frame_width / gradient_steps;
        
        if (client->is_active) {
            r = 0x2A * (1 - t) + 0xE8 * t;
            g = 0x4D * (1 - t) + 0xE8 * t;
            b = 0xA9 * (1 - t) + 0xE8 * t;
        } else {
            r = 0xE8 * (1 - t) + 0x00 * t;
            g = 0xE8 * (1 - t) + 0x00 * t;
            b = 0xE8 * (1 - t) + 0x00 * t;
        }
        
        int color = (r << 16) | (g << 8) | b;
        XSetForeground(dpy, gc, color);
        XFillRectangle(dpy, win, gc, x1, 0, x2 - x1 + 1, frame_height);
    }
    
    if (client->is_active) {
        XSetForeground(dpy, wm->border_gc, 0x2A4DA9);
    } else {
        XSetForeground(dpy, wm->border_gc, 0x333333);
    }
    XDrawRectangle(dpy, win, wm->border_gc, 0, 0, frame_width, frame_height);
    
    XSetForeground(dpy, gc, client->is_active ? 0x8CA8FF : 0xAAAAAA);
    XDrawLine(dpy, win, gc, 1, 1, frame_width - 2, 1);
    XDrawLine(dpy, win, gc, 1, 1, 1, frame_height - 2);
    
    int titlebar_top_color, titlebar_bottom_color;
    if (client->is_active) {
        titlebar_top_color = 0x3A6DD8;
        titlebar_bottom_color = 0x2A4DA8;
    } else {
        titlebar_top_color = 0xCCCCCC;
        titlebar_bottom_color = 0xAAAAAA;
    }
    
    int titlebar_gradient_steps = 8;
    for (int i = 0; i < titlebar_gradient_steps; i++) {
        float t = (float)i / (titlebar_gradient_steps - 1);
        int r = ((titlebar_top_color >> 16) & 0xFF) * (1 - t) + ((titlebar_bottom_color >> 16) & 0xFF) * t;
        int g = ((titlebar_top_color >> 8) & 0xFF) * (1 - t) + ((titlebar_bottom_color >> 8) & 0xFF) * t;
        int b = (titlebar_top_color & 0xFF) * (1 - t) + (titlebar_bottom_color & 0xFF) * t;
        int color = (r << 16) | (g << 8) | b;
        
        XSetForeground(dpy, gc, color);
        int y1 = BORDER_WIDTH + i * (TITLEBAR_HEIGHT - BORDER_WIDTH) / titlebar_gradient_steps;
        int y2 = BORDER_WIDTH + (i + 1) * (TITLEBAR_HEIGHT - BORDER_WIDTH) / titlebar_gradient_steps;
        XFillRectangle(dpy, win, gc, BORDER_WIDTH, y1, 
                      frame_width - 2 * BORDER_WIDTH, y2 - y1 + 1);
    }
    
    XSetForeground(dpy, gc, client->is_active ? 0x1A3D88 : 0x888888);
    XDrawRectangle(dpy, win, gc, 
                   BORDER_WIDTH, 
                   BORDER_WIDTH,
                   frame_width - 2 * BORDER_WIDTH,
                   TITLEBAR_HEIGHT - BORDER_WIDTH);
    
    XSetForeground(dpy, gc, client->is_active ? 0x2A4DA9 : 0x666666);
    XDrawLine(dpy, win, gc, 
              0, 
              TITLEBAR_HEIGHT,
              frame_width,
              TITLEBAR_HEIGHT);
    
    XSetForeground(dpy, gc, client->is_active ? 0x8CA8FF : 0xFFFFFF);
    XDrawLine(dpy, win, gc, 
              0, 
              TITLEBAR_HEIGHT + 1,
              frame_width,
              TITLEBAR_HEIGHT + 1);
    
    int close_x = BORDER_WIDTH + BUTTON_SPACING;
    int close_y = BORDER_WIDTH + (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
    int maximize_x = frame_width - BORDER_WIDTH - BUTTON_SPACING - 2*BUTTON_SIZE - 2*BUTTON_SPACING;
    int maximize_y = close_y;
    int minimize_x = frame_width - BORDER_WIDTH - BUTTON_SPACING - BUTTON_SIZE - BUTTON_SPACING;
    int minimize_y = close_y;
    
    draw_button(wm, win, close_x, close_y, 0, 0, 0, client->is_active);
    draw_button(wm, win, maximize_x, maximize_y, 0, 0, 1, client->is_active);
    draw_button(wm, win, minimize_x, minimize_y, 0, 0, 2, client->is_active);
}

void draw_frame(AquaWm *wm, Client *client) {
    draw_window(wm, client);
}

void send_configure_notify(AquaWm *wm, Client *client) {
    XEvent ev;
    
    memset(&ev, 0, sizeof(ev));
    ev.type = ConfigureNotify;
    ev.xconfigure.serial = 0;
    ev.xconfigure.send_event = True;
    ev.xconfigure.display = wm->display;
    ev.xconfigure.event = client->window;
    ev.xconfigure.window = client->window;
    ev.xconfigure.x = client->x;
    ev.xconfigure.y = client->y;
    ev.xconfigure.width = client->width;
    ev.xconfigure.height = client->height;
    ev.xconfigure.border_width = 0;
    ev.xconfigure.above = None;
    ev.xconfigure.override_redirect = False;
    
    XSendEvent(wm->display, client->window, False, StructureNotifyMask, &ev);
}

void set_focus_safe(AquaWm *wm, Window window) {
    XWindowAttributes attr;
    if (XGetWindowAttributes(wm->display, window, &attr)) {
        if (attr.map_state == IsViewable) {
            XSetInputFocus(wm->display, window, RevertToParent, CurrentTime);
        }
    }
}

void handle_map_notify(AquaWm *wm, XEvent *e) {
    XMapEvent *ev = &e->xmap;
    
    Client *client = find_client_by_window(wm, ev->window);
    if (client) {
        client->mapped = True;
        client->ignore_unmap = 0;
        
        if (client->is_dock) {
            enforce_dock_above(wm, client);
            XRaiseWindow(wm->display, client->window);
            update_dock_struts(wm);
            keep_docks_on_top(wm);
        } else {
            if (!client->decorated) {
                XRaiseWindow(wm->display, client->window);
            } else {
                if (!wm->active_client) {
                    wm->active_client = client;
                    client->is_active = 1;
                }
                
                if (!client->is_fullscreen) {
                    draw_frame(wm, client);
                }
                set_focus_safe(wm, client->window);
            }
        }
        
        keep_docks_on_top(wm);
    }
}

void handle_map_request(AquaWm *wm, XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;
    
    Client *existing = find_client_by_window(wm, ev->window);
    if (existing) {
        if (existing->is_dock) {
            XMapWindow(wm->display, existing->window);
            enforce_dock_above(wm, existing);
        } else if (existing->decorated && !existing->is_fullscreen) {
            XMapWindow(wm->display, existing->frame);
        } else {
            XMapWindow(wm->display, existing->window);
        }
        existing->mapped = True;
        keep_docks_on_top(wm);
        return;
    }
    
    Client *client = calloc(1, sizeof(Client));
    client->window = ev->window;
    
    create_frame(wm, client);
    
    if (client->is_dock) {
        XMapWindow(wm->display, client->window);
    } else {
        XMapWindow(wm->display, client->window);
    }
    
    keep_docks_on_top(wm);
}

void handle_configure_request(AquaWm *wm, XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    
    Client *client = find_client_by_window(wm, ev->window);
    
    if (client) {
        if (client->is_dock) {
            XWindowChanges wc;
            wc.x = ev->x;
            wc.y = ev->y;
            wc.width = ev->width;
            wc.height = ev->height;
            wc.border_width = ev->border_width;
            wc.sibling = ev->above;
            wc.stack_mode = ev->detail;
            
            XConfigureWindow(wm->display, ev->window, ev->value_mask, &wc);
            
            client->x = ev->x;
            client->y = ev->y;
            client->width = ev->width;
            client->height = ev->height;
            
            update_dock_struts(wm);
            keep_docks_on_top(wm);
            return;
        }
        
        if (client->is_fullscreen) {
            return;
        }
        
        if (client->decorated) {
            if (ev->value_mask & CWWidth) {
                client->width = ev->width;
                if (client->width < MIN_WIDTH) client->width = MIN_WIDTH;
            }
            if (ev->value_mask & CWHeight) {
                client->height = ev->height;
                if (client->height < MIN_HEIGHT) client->height = MIN_HEIGHT;
            }
            
            if (!client->is_maximized && !wm->is_moving && !wm->is_resizing) {
                if (ev->value_mask & CWX) client->x = ev->x;
                if (ev->value_mask & CWY) client->y = ev->y;
                
                client->frame_x = client->x - BORDER_WIDTH;
                client->frame_y = client->y - TITLEBAR_HEIGHT;
            }
            
            apply_window_state(wm, client);
            if (!client->is_fullscreen) {
                draw_frame(wm, client);
            }
            send_configure_notify(wm, client);
        } else {
            XWindowChanges wc;
            wc.x = ev->x;
            wc.y = ev->y;
            wc.width = ev->width;
            wc.height = ev->height;
            wc.border_width = ev->border_width;
            wc.sibling = ev->above;
            wc.stack_mode = ev->detail;
            
            XConfigureWindow(wm->display, ev->window, ev->value_mask, &wc);
        }
        
        keep_docks_on_top(wm);
    } else {
        XWindowChanges wc;
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        
        XConfigureWindow(wm->display, ev->window, ev->value_mask, &wc);
    }
}

void handle_button_press(AquaWm *wm, XEvent *e) {
    XButtonEvent *ev = &e->xbutton;
    Client *client = find_client_by_frame(wm, ev->window);
    
    if (wm->mouse_grabbed && wm->grab_window != ev->window) {
        return;
    }
    
    if (!client) {
        client = find_client_by_window(wm, ev->window);
        if (client) {
            if (client->is_dock) {
                keep_docks_on_top(wm);
                return;
            }
            
            if (client->decorated) {
                wm->is_moving = 1;
                wm->moving_client = client;
                wm->move_start_x = ev->x_root - client->frame_x;
                wm->move_start_y = ev->y_root - client->frame_y;
                wm->drag_button = ev->button;
                wm->mouse_grabbed = 1;
                wm->grab_window = client->frame;
                wm->grabbed_client = client;
                
                XGrabPointer(wm->display, client->frame, True,
                            ButtonMotionMask | ButtonReleaseMask,
                            GrabModeAsync, GrabModeAsync,
                            None, wm->cursor_move, CurrentTime);
            }
        }
        return;
    }
    
    if (client->decorated && !client->is_fullscreen && !client->is_dock) {
        XRaiseWindow(wm->display, client->frame);
        
        if (wm->active_client != client) {
            if (wm->active_client) {
                wm->active_client->is_active = 0;
                if (wm->active_client->decorated && !wm->active_client->is_fullscreen) {
                    draw_frame(wm, wm->active_client);
                }
            }
            wm->active_client = client;
            client->is_active = 1;
            draw_frame(wm, client);
        }
        
        set_focus_safe(wm, client->window);
        wm->drag_button = ev->button;
        
        int local_x = ev->x;
        int local_y = ev->y;
        
        if (local_y < TITLEBAR_HEIGHT) {
            int frame_width = client->width + 2 * BORDER_WIDTH;
            int close_x = BORDER_WIDTH + BUTTON_SPACING;
            int close_y = BORDER_WIDTH + (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
            int maximize_x = frame_width - BORDER_WIDTH - BUTTON_SPACING - 2*BUTTON_SIZE - 2*BUTTON_SPACING;
            int maximize_y = close_y;
            int minimize_x = frame_width - BORDER_WIDTH - BUTTON_SPACING - BUTTON_SIZE - BUTTON_SPACING;
            int minimize_y = close_y;
            
            if (is_over_button(local_x, local_y, close_x, close_y)) {
                XEvent ce;
                memset(&ce, 0, sizeof(ce));
                ce.type = ClientMessage;
                ce.xclient.window = client->window;
                ce.xclient.message_type = wm->wm_protocols;
                ce.xclient.format = 32;
                ce.xclient.data.l[0] = wm->wm_delete_window;
                ce.xclient.data.l[1] = CurrentTime;
                
                XSendEvent(wm->display, client->window, False, NoEventMask, &ce);
                return;
            } else if (is_over_button(local_x, local_y, maximize_x, maximize_y)) {
                toggle_maximize(wm, client);
                return;
            } else if (is_over_button(local_x, local_y, minimize_x, minimize_y)) {
                minimize_window(wm, client);
                return;
            }
            
            wm->is_moving = 1;
            wm->moving_client = client;
            wm->move_start_x = ev->x_root - client->frame_x;
            wm->move_start_y = ev->y_root - client->frame_y;
            wm->mouse_grabbed = 1;
            wm->grab_window = client->frame;
            wm->grabbed_client = client;
            
            XGrabPointer(wm->display, client->frame, True,
                        ButtonMotionMask | ButtonReleaseMask,
                        GrabModeAsync, GrabModeAsync,
                        None, wm->cursor_move, CurrentTime);
        } else {
            int edge = get_resize_edge(client, local_x, local_y);
            if (edge != 0) {
                wm->is_resizing = 1;
                wm->resize_edge = edge;
                wm->moving_client = client;
                wm->drag_start_x = ev->x_root;
                wm->drag_start_y = ev->y_root;
                client->orig_x = client->x;
                client->orig_y = client->y;
                client->orig_width = client->width;
                client->orig_height = client->height;
                wm->mouse_grabbed = 1;
                wm->grab_window = client->frame;
                wm->grabbed_client = client;
                
                XGrabPointer(wm->display, client->frame, True,
                            ButtonMotionMask | ButtonReleaseMask,
                            GrabModeAsync, GrabModeAsync,
                            None, wm->cursor_resize, CurrentTime);
            }
        }
        
        keep_docks_on_top(wm);
    }
}

void handle_motion_notify(AquaWm *wm, XEvent *e) {
    XMotionEvent *ev = &e->xmotion;
    
    if (!wm->mouse_grabbed || !wm->moving_client) return;
    
    Client *client = wm->moving_client;
    
    if (!client || client->window == None) {
        wm->is_moving = 0;
        wm->is_resizing = 0;
        wm->moving_client = NULL;
        wm->mouse_grabbed = 0;
        wm->grab_window = None;
        XUngrabPointer(wm->display, CurrentTime);
        return;
    }
    
    if (client->decorated && !client->is_fullscreen && !client->is_dock) {
        if (wm->is_moving && !client->is_maximized) {
            int new_x = ev->x_root - wm->move_start_x;
	    int new_y = ev->y_root - wm->move_start_y;

            int frame_width = client->width + 2 * BORDER_WIDTH;
            int frame_height = client->height + TITLEBAR_HEIGHT + BORDER_WIDTH;
            
            if (new_x < wm->workarea_x) new_x = wm->workarea_x;
            if (new_y < wm->workarea_y) new_y = wm->workarea_y;
            if (new_x + frame_width > wm->workarea_x + wm->workarea_width)
                new_x = wm->workarea_x + wm->workarea_width - frame_width;
            if (new_y + frame_height > wm->workarea_y + wm->workarea_height)
                new_y = wm->workarea_y + wm->workarea_height - frame_height;
            
            client->frame_x = new_x;
            client->frame_y = new_y;
            client->x = new_x + BORDER_WIDTH;
            client->y = new_y + TITLEBAR_HEIGHT;
            
            static int last_x = 0, last_y = 0;
            if (abs(new_x - last_x) > 2 || abs(new_y - last_y) > 2) {
                XMoveWindow(wm->display, client->frame, new_x, new_y);
                send_configure_notify(wm, client);
                last_x = new_x;
                last_y = new_y;
            }
            
            update_cursor(wm, client->frame, ev->x, ev->y);
        } else if (wm->is_resizing && !client->is_maximized) {
            int dx = ev->x_root - wm->drag_start_x;
            int dy = ev->x_root - wm->drag_start_y;
            
            int new_x = client->orig_x;
            int new_y = client->orig_y;
            int new_width = client->orig_width;
            int new_height = client->orig_height;
            
            if (wm->resize_edge & 1) {
                new_x = client->orig_x + dx;
                new_width = client->orig_width - dx;
                if (new_width < MIN_WIDTH) {
                    new_width = MIN_WIDTH;
                    new_x = client->orig_x + client->orig_width - MIN_WIDTH;
                }
                if (new_x < wm->workarea_x + BORDER_WIDTH) {
                    new_x = wm->workarea_x + BORDER_WIDTH;
                    new_width = client->orig_x + client->orig_width - wm->workarea_x - BORDER_WIDTH;
                }
            }
            if (wm->resize_edge & 2) {
                new_width = client->orig_width + dx;
                if (new_width < MIN_WIDTH) new_width = MIN_WIDTH;
                if (new_width > wm->workarea_width - (new_x - wm->workarea_x) - 2 * BORDER_WIDTH)
                    new_width = wm->workarea_width - (new_x - wm->workarea_x) - 2 * BORDER_WIDTH;
            }
            if (wm->resize_edge & 4) {
                new_y = client->orig_y + dy;
                new_height = client->orig_height - dy;
                if (new_height < MIN_HEIGHT) {
                    new_height = MIN_HEIGHT;
                    new_y = client->orig_y + client->orig_height - MIN_HEIGHT;
                }
                if (new_y < wm->workarea_y + TITLEBAR_HEIGHT) {
                    new_y = wm->workarea_y + TITLEBAR_HEIGHT;
                    new_height = client->orig_y + client->orig_height - wm->workarea_y - TITLEBAR_HEIGHT;
                }
            }
            if (wm->resize_edge & 8) {
                new_height = client->orig_height + dy;
                if (new_height < MIN_HEIGHT) new_height = MIN_HEIGHT;
                if (new_height > wm->workarea_height - (new_y - wm->workarea_y) - TITLEBAR_HEIGHT)
                    new_height = wm->workarea_height - (new_y - wm->workarea_y) - TITLEBAR_HEIGHT;
            }
            
            client->x = new_x;
            client->y = new_y;
            client->width = new_width;
            client->height = new_height;
            
            client->frame_x = client->x - BORDER_WIDTH;
            client->frame_y = client->y - TITLEBAR_HEIGHT;
            
            int frame_width = client->width + 2 * BORDER_WIDTH;
            int frame_height = client->height + TITLEBAR_HEIGHT + BORDER_WIDTH;
            
            static int last_width = 0, last_height = 0;
            if (abs(frame_width - last_width) > 4 || abs(frame_height - last_height) > 4) {
                XMoveResizeWindow(wm->display, client->frame,
                                 client->frame_x, client->frame_y,
                                 frame_width, frame_height);
                
                XResizeWindow(wm->display, client->window, client->width, client->height);
                send_configure_notify(wm, client);
                
                last_width = frame_width;
                last_height = frame_height;
            }
            
            update_cursor(wm, client->frame, ev->x, ev->y);
        }
        
        keep_docks_on_top(wm);
    }
}

void handle_button_release(AquaWm *wm, XEvent *e) {
    XButtonEvent *ev = &e->xbutton;
    
    if (ev->button == wm->drag_button && (wm->is_moving || wm->is_resizing)) {
        XUngrabPointer(wm->display, CurrentTime);
        wm->is_moving = 0;
        wm->is_resizing = 0;
        wm->resize_edge = 0;
        wm->mouse_grabbed = 0;
        wm->grab_window = None;
        
        if (wm->grabbed_client && wm->grabbed_client->decorated && !wm->grabbed_client->is_fullscreen) {
            draw_frame(wm, wm->grabbed_client);
            wm->grabbed_client = NULL;
        }
        
        wm->moving_client = NULL;
        wm->drag_button = 0;
        
        keep_docks_on_top(wm);
    }
}

void handle_key_press(AquaWm *wm, XEvent *e) {
    XKeyEvent *ev = &e->xkey;
    
    if ((ev->state & Mod4Mask) && ev->keycode == XKeysymToKeycode(wm->display, XK_F4)) {
        if (wm->active_client && wm->active_client->decorated) {
            XEvent ce;
            memset(&ce, 0, sizeof(ce));
            ce.type = ClientMessage;
            ce.xclient.window = wm->active_client->window;
            ce.xclient.message_type = wm->wm_protocols;
            ce.xclient.format = 32;
            ce.xclient.data.l[0] = wm->wm_delete_window;
            ce.xclient.data.l[1] = CurrentTime;
            
            XSendEvent(wm->display, wm->active_client->window, False, NoEventMask, &ce);
        }
    } else if ((ev->state & Mod4Mask) && ev->keycode == XKeysymToKeycode(wm->display, XK_Tab)) {
        if (wm->num_clients > 1) {
            int next_index = 0;
            for (int i = 0; i < wm->num_clients; i++) {
                if (wm->active_client == wm->clients[i] && wm->clients[i]->decorated) {
                    next_index = (i + 1) % wm->num_clients;
                    while (!wm->clients[next_index]->decorated || wm->clients[next_index]->is_dock) {
                        next_index = (next_index + 1) % wm->num_clients;
                    }
                    break;
                }
            }
            
            if (wm->active_client) {
                wm->active_client->is_active = 0;
                if (wm->active_client->decorated && !wm->active_client->is_fullscreen) {
                    draw_frame(wm, wm->active_client);
                }
            }
            
            wm->active_client = wm->clients[next_index];
            wm->active_client->is_active = 1;
            if (wm->active_client->decorated && !wm->active_client->is_fullscreen) {
                XRaiseWindow(wm->display, wm->active_client->frame);
                draw_frame(wm, wm->active_client);
            }
            set_focus_safe(wm, wm->active_client->window);
        }
    } else if (ev->keycode == XKeysymToKeycode(wm->display, XK_F11)) {
        if (wm->active_client && wm->active_client->decorated) {
            toggle_fullscreen(wm, wm->active_client);
        }
    }
    
    keep_docks_on_top(wm);
}

void handle_client_message(AquaWm *wm, XEvent *e) {
    XClientMessageEvent *ev = &e->xclient;
    
    if (ev->message_type == wm->wm_protocols) {
        if ((Atom)ev->data.l[0] == wm->wm_delete_window) {
            Client *client = find_client_by_window(wm, ev->window);
            if (client) {
                client->ignore_unmap = 1;
                XDestroyWindow(wm->display, client->window);
            }
        } else if ((Atom)ev->data.l[0] == wm->wm_take_focus) {
            Client *client = find_client_by_window(wm, ev->window);
            if (client) {
                set_focus_safe(wm, client->window);
            }
        }
    } else if (ev->message_type == wm->net_wm_state) {
        Client *client = find_client_by_window(wm, ev->window);
        if (client && client->decorated) {
            for (int i = 1; i < 3; i++) {
                Atom atom = ev->data.l[i];
                if (atom == wm->net_wm_state_maximized_horz || 
                    atom == wm->net_wm_state_maximized_vert) {
                    toggle_maximize(wm, client);
                    break;
                } else if (atom == wm->net_wm_state_fullscreen) {
                    toggle_fullscreen(wm, client);
                    break;
                }
            }
        }
    }
    
    keep_docks_on_top(wm);
}

void handle_destroy_notify(AquaWm *wm, XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    
    Client *client = find_client_by_window(wm, ev->window);
    if (client) {
        remove_client(wm, client);
    }
    
    keep_docks_on_top(wm);
}

void handle_unmap_notify(AquaWm *wm, XEvent *e) {
    XUnmapEvent *ev = &e->xunmap;
    
    Client *client = find_client_by_window(wm, ev->window);
    if (client) {
        if (client->ignore_unmap) {
            client->ignore_unmap = 0;
            return;
        }
        
        if (client->decorated && !client->is_fullscreen) {
            XUnmapWindow(wm->display, client->frame);
        }
        client->mapped = False;
    }
    
    keep_docks_on_top(wm);
}

void handle_expose(AquaWm *wm, XEvent *e) {
    XExposeEvent *ev = &e->xexpose;
    
    Client *client = find_client_by_frame(wm, ev->window);
    if (client && client->decorated && !client->is_fullscreen) {
        draw_frame(wm, client);
    }
    
    keep_docks_on_top(wm);
}

void handle_enter_notify(AquaWm *wm, XEvent *e) {
    XEnterWindowEvent *ev = &e->xcrossing;
    
    Client *client = find_client_by_frame(wm, ev->window);
    if (client && client->decorated && !client->is_fullscreen) {
        update_cursor(wm, ev->window, ev->x, ev->y);
        draw_frame(wm, client);
    }
}

void handle_leave_notify(AquaWm *wm, XEvent *e) {
    XLeaveWindowEvent *ev = &e->xcrossing;
    
    Client *client = find_client_by_frame(wm, ev->window);
    if (client && client->decorated && !client->is_fullscreen) {
        XDefineCursor(wm->display, ev->window, wm->cursor_normal);
        draw_frame(wm, client);
    }
}

Client* find_client_by_window(AquaWm *wm, Window win) {
    for (int i = 0; i < wm->num_clients; i++) {
        if (wm->clients[i]->window == win) {
            return wm->clients[i];
        }
    }
    return NULL;
}

Client* find_client_by_frame(AquaWm *wm, Window frame) {
    for (int i = 0; i < wm->num_clients; i++) {
        if (wm->clients[i]->frame == frame && wm->clients[i]->decorated) {
            return wm->clients[i];
        }
    }
    return NULL;
}

void remove_client(AquaWm *wm, Client *client) {
    if (!client) return;
    
    int index = -1;
    for (int i = 0; i < wm->num_clients; i++) {
        if (wm->clients[i] == client) {
            index = i;
            break;
        }
    }
    
    if (index >= 0) {
        if (wm->moving_client == client || wm->grabbed_client == client) {
            wm->is_moving = 0;
            wm->is_resizing = 0;
            wm->moving_client = NULL;
            wm->grabbed_client = NULL;
            wm->mouse_grabbed = 0;
            wm->grab_window = None;
            XUngrabPointer(wm->display, CurrentTime);
        }
        
        if (client->title) free(client->title);
        
        for (int i = index; i < wm->num_clients - 1; i++) {
            wm->clients[i] = wm->clients[i + 1];
        }
        
        wm->num_clients--;
        if (wm->num_clients > 0) {
            wm->clients = realloc(wm->clients, wm->num_clients * sizeof(Client*));
        } else {
            free(wm->clients);
            wm->clients = NULL;
        }
        
        if (wm->active_client == client) {
            wm->active_client = NULL;
            for (int i = 0; i < wm->num_clients; i++) {
                if (wm->clients[i]->decorated && !wm->clients[i]->is_dock) {
                    wm->active_client = wm->clients[i];
                    wm->active_client->is_active = 1;
                    set_focus_safe(wm, wm->active_client->window);
                    if (!wm->active_client->is_fullscreen) {
                        draw_frame(wm, wm->active_client);
                    }
                    break;
                }
            }
        }
        
        free(client);
    }
    
    update_dock_struts(wm);
    keep_docks_on_top(wm);
}

void aquawm_run(AquaWm *wm) {
    XEvent ev;
    
    printf("AquaWM started\n");
    
    keep_docks_on_top(wm);
    
    while (1) {
        XNextEvent(wm->display, &ev);
        
        switch (ev.type) {
            case MapRequest:
                handle_map_request(wm, &ev);
                break;
            case MapNotify:
                handle_map_notify(wm, &ev);
                break;
            case ConfigureRequest:
                handle_configure_request(wm, &ev);
                break;
            case ButtonPress:
                handle_button_press(wm, &ev);
                break;
            case MotionNotify:
                handle_motion_notify(wm, &ev);
                break;
            case ButtonRelease:
                handle_button_release(wm, &ev);
                break;
            case KeyPress:
                handle_key_press(wm, &ev);
                break;
            case ClientMessage:
                handle_client_message(wm, &ev);
                break;
            case DestroyNotify:
                handle_destroy_notify(wm, &ev);
                break;
            case UnmapNotify:
                handle_unmap_notify(wm, &ev);
                break;
            case Expose:
                handle_expose(wm, &ev);
                break;
            case EnterNotify:
                handle_enter_notify(wm, &ev);
                break;
            case LeaveNotify:
                handle_leave_notify(wm, &ev);
                break;
            default:
                break;
        }
        
        XFlush(wm->display);
    }
}

void sigint_handler(int sig) {
    (void)sig;
    printf("\nAquaWM shutting down!!\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    signal(SIGINT, sigint_handler);
    
    AquaWm *wm = aquawm_init();
    if (!wm) {
        fprintf(stderr, "Failed to initialize WM!!\n");
        return 1;
    }
    
    aquawm_run(wm);
    
    if (wm->frame_gc) XFreeGC(wm->display, wm->frame_gc);
    if (wm->button_gc) XFreeGC(wm->display, wm->button_gc);
    if (wm->border_gc) XFreeGC(wm->display, wm->border_gc);
    if (wm->desktop_gc) XFreeGC(wm->display, wm->desktop_gc);
    if (wm->desktop_pixmap != None) XFreePixmap(wm->display, wm->desktop_pixmap);
    
    XCloseDisplay(wm->display);
    free(wm);
    
    return 0;
}
