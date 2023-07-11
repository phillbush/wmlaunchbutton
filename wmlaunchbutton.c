#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#define LEN(a)          (sizeof(a) / sizeof((a)[0]))
#define FLAG(f, b)      (((f) & (b)) == (b))
#define CLASS           "WMLaunchButton"
#define NAME            "wmlaunchbutton"

enum {
	BUTTON_INACTIVE,
	BUTTON_HOVERED,
	BUTTON_ACTIVE,
	NBUTTONS
};

struct Button {
	Pixmap pixmap, mask;
	int width, height;
	bool needtofree;
};

struct Rectangle {
	int x, y, width, height;
};

static void
usage(void)
{
	(void)fprintf(stderr, "usage: wmlaunchbutton pixmap [pixmap [pixmap]] command");
	exit(EXIT_FAILURE);
}

static void
spawncmd(struct Rectangle *geometry, char *cmd)
{
	pid_t pid;
	char buf[64];

	snprintf(
		buf, sizeof(buf),
		"%dx%d%+d%+d",
		geometry->width,
		geometry->height,
		geometry->x,
		geometry->y
	);
	setenv("BUTTON_GEOMETRY", buf, 1);
	switch (pid = fork()) {
	case -1:
		warn("fork");
		break;
	case 0:         /* child */
		execvp(
			"sh",
			(char *[]){"sh", "-c", cmd, NULL}
		);
		err(EXIT_FAILURE, "%s", cmd);
	default:        /* parent */
		while (waitpid(pid, NULL, 0) == -1)
			if (errno != EINTR)
				err(EXIT_FAILURE, "waitpid");
		break;
	}
}

static void
getpixmap(Display *display, Window window, struct Button *button, const char *filename)
{
	XpmAttributes xattr = { 0 };
	int status;

	status = XpmReadFileToPixmap(
		display,
		window,
		filename,
		&button->pixmap,
		&button->mask,
		&xattr
	);
	if (status != XpmSuccess || !FLAG(xattr.valuemask, XpmSize))
		errx(EXIT_FAILURE, "could not load pixmap");
	button->width = xattr.width;
	button->height = xattr.height;
	button->needtofree = true;
}

static void
setbackground(Display *display, Window window, struct Button *button)
{
	XShapeCombineMask(display, window, ShapeClip, 0, 0, button->mask, ShapeSet);
	XShapeCombineMask(display, window, ShapeBounding, 0, 0, button->mask, ShapeSet);
	XSetWindowBackgroundPixmap(display, window, button->pixmap);
	XClearWindow(display, window);
	XFlush(display);
}

static void
run(Display *display, Window window, struct Button *buttons, char *cmd)
{
	struct Rectangle geometry = { 0 };
	XEvent xevent;

	geometry.width = buttons[BUTTON_INACTIVE].width;
	geometry.height = buttons[BUTTON_INACTIVE].height;
	setbackground(display, window, &buttons[BUTTON_INACTIVE]);
	XMapWindow(display, window);
	for (;;) {
		XNextEvent(display, &xevent);
		switch (xevent.type) {
		case EnterNotify:
			setbackground(display, window, &buttons[BUTTON_HOVERED]);
			break;
		case LeaveNotify:
			setbackground(display, window, &buttons[BUTTON_INACTIVE]);
			break;
		case ButtonPress:
			if (xevent.xbutton.button != Button1)
				break;
			XUngrabPointer(display, xevent.xbutton.time);
			setbackground(display, window, &buttons[BUTTON_ACTIVE]);
			spawncmd(&geometry, cmd);
			setbackground(display, window, &buttons[BUTTON_INACTIVE]);
			while (XPending(display) > 0)
				XNextEvent(display, &xevent);
			break;
		case ConfigureNotify:
			geometry.x = xevent.xconfigure.x;
			geometry.y = xevent.xconfigure.y;
			geometry.width = xevent.xconfigure.width;
			geometry.height = xevent.xconfigure.height;
			break;
		default:
			break;
		}
	}
}

int
main(int argc, char *argv[])
{
	Display *display;
	Window window;
	int i;
	struct Button buttons[NBUTTONS] = { { 0 } };

	if (argc < 3 || argc > 5)
		usage();
	for (i = 0; i < argc; i++)
		if (argv[i] == NULL || argv[i][0] == '\0')
			errx(EXIT_FAILURE, "empty argument");
	if ((display = XOpenDisplay(NULL)) == NULL)
		errx(EXIT_FAILURE, "could not connect to X server");
	window = XCreateWindow(
		display,
		DefaultRootWindow(display),
		0, 0, 1, 1,
		0,
		CopyFromParent, CopyFromParent, CopyFromParent,
		CWEventMask,
		&(XSetWindowAttributes){
			.event_mask = EnterWindowMask | LeaveWindowMask
			            | StructureNotifyMask | ButtonPressMask,
		}
	);
	getpixmap(display, window, &buttons[BUTTON_INACTIVE], argv[1]);
	buttons[BUTTON_HOVERED].pixmap = buttons[BUTTON_INACTIVE].pixmap;
	buttons[BUTTON_HOVERED].mask = buttons[BUTTON_INACTIVE].mask;
	if (argc > 3) {
		getpixmap(display, window, &buttons[BUTTON_HOVERED], argv[2]);
		if (buttons[BUTTON_HOVERED].width != buttons[BUTTON_INACTIVE].width)
			errx(EXIT_FAILURE, "pixmaps with different sizes");
		if (buttons[BUTTON_HOVERED].height != buttons[BUTTON_INACTIVE].height)
			errx(EXIT_FAILURE, "pixmaps with different sizes");
	}
	buttons[BUTTON_ACTIVE].pixmap = buttons[BUTTON_HOVERED].pixmap;
	buttons[BUTTON_ACTIVE].mask = buttons[BUTTON_HOVERED].mask;
	if (argc > 4) {
		getpixmap(display, window, &buttons[BUTTON_ACTIVE], argv[3]);
		if (buttons[BUTTON_ACTIVE].width != buttons[BUTTON_HOVERED].width)
			errx(EXIT_FAILURE, "pixmaps with different sizes");
		if (buttons[BUTTON_ACTIVE].height != buttons[BUTTON_HOVERED].height)
			errx(EXIT_FAILURE, "pixmaps with different sizes");
	}
	XResizeWindow(
		display,
		window,
		buttons[BUTTON_INACTIVE].width,
		buttons[BUTTON_INACTIVE].height
	);
	XmbSetWMProperties(
		display, window,
		CLASS, CLASS,
		argv, argc,
		&(XSizeHints){
			.flags = PMaxSize | PMinSize,
			.min_width = buttons[BUTTON_INACTIVE].width,
			.max_width = buttons[BUTTON_INACTIVE].width,
			.min_height = buttons[BUTTON_INACTIVE].height,
			.max_height = buttons[BUTTON_INACTIVE].height,
		},
		&(XWMHints){
			.flags = IconWindowHint | StateHint | WindowGroupHint,
			.initial_state = WithdrawnState,
			.window_group = window,
			.icon_window = window,
		},
		&(XClassHint){
			.res_class = CLASS,
			.res_name = NAME,
		}
	);
	run(display, window, buttons, argv[argc-1]);
	return EXIT_FAILURE;    /* unreachable */
}
