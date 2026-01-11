AquaShell is simple yet powerful desktop environment for X11.All programs in this project written in C using GTK.Aquashell can be somewhat similar to older Mac OSX desktops, but it doesnt attempt to replicate it.
 
# Current features
6 desktops.
Fully operational buttons on the window frame.
NOTE: windows are not torn from the window frame for security measures.
# Dependencies
    any C compiler(gcc, clang, ...)
    gtk2/3
    Xlib
    Xutil
    Xatom
# Compile
    sudo/doas make install
# Run
Add aquawm file in your xsessions directory.
Or simply run it via startx:
    
    <.xinitrc>
    exec aquawm
 
 
