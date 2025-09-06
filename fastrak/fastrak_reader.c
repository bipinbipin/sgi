/*
 * Fastrak 3SP Reader for SGI IRIX
 * Reads position and orientation data from Polhemus Fastrak 3SP
 * via serial port connection
 cc -O2 -fullwarn -o fastrak_reader fastrak_reader.c -lGL -lGLU -lX11 -lm -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <pthread.h>

/* Fastrak 3SP Constants */
#define FASTRAK_BAUD B19200
#define MAX_BUFFER 256
#define MAX_SENSORS 4
#define DEFAULT_DEVICE "/dev/ttyd1"  /* Common SGI serial port */

/* Fastrak Commands */
#define CMD_CONTINUOUS "C\r"         /* Continuous output mode */
#define CMD_POINT "P\r"              /* Point mode (single reading) */
#define CMD_BINARY "F\r"             /* Binary output format */
#define CMD_ASCII "A\r"              /* ASCII output format */
#define CMD_RESET "W\r"              /* Reset to defaults */
#define CMD_OUTPUT_LIST "O1,2,8,7,11\r" /* Output list: X,Y,Z,Az,El,Roll for station 1 */
#define CMD_ENABLE_STATION "l1,1\r"  /* Enable station 1 */

/* Fastrak data structure */
typedef struct {
    int station;
    float x, y, z;        /* Position in inches */
    float azimuth, elevation, roll;  /* Orientation in degrees */
    int valid;
} FastrakData;

/* Function prototypes */
void display_data(const FastrakData *data);
void draw_cube(void);
void render_scene(void);
void setup_viewport(int width, int height);
int init_x11_opengl(void);
void opengl_event_loop(void);
void* fastrak_thread(void* arg);

/* Global variables */
int serial_fd = -1;
int running = 1;

/* OpenGL and threading variables */
FastrakData current_data = {0};
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
int use_opengl = 0;

/* X11/OpenGL variables */
Display *display = NULL;
Window window;
GLXContext context;
int window_width = 800;
int window_height = 600;

/* Graph variables */
#define GRAPH_HISTORY 200
float x_history[GRAPH_HISTORY];
int history_index = 0;
int history_count = 0;

/* Signal handler for clean exit */
void signal_handler(int sig) {
    (void)sig; /* Suppress unused parameter warning */
    printf("\nShutting down Fastrak reader...\n");
    running = 0;
}

/* Configure serial port for Fastrak communication */
int configure_serial_port(int fd) {
    struct termios options;
    
    /* Get current port settings */
    if (tcgetattr(fd, &options) < 0) {
        perror("tcgetattr");
        return -1;
    }
    
    /* Set baud rate */
    cfsetispeed(&options, FASTRAK_BAUD);
    cfsetospeed(&options, FASTRAK_BAUD);
    
    /* Configure port settings for Fastrak 3SP:
     * 8 data bits, 1 parity bit, 0 stop bits, no flow control */
    options.c_cflag |= PARENB;       /* Enable parity */
    options.c_cflag &= ~PARODD;      /* Even parity (you may need to adjust if odd parity) */
    options.c_cflag &= ~CSTOPB;      /* 1 stop bit (0 stop bits not standard - using 1) */
    options.c_cflag &= ~CSIZE;       /* Clear data size bits */
    options.c_cflag |= CS8;          /* 8 data bits */
    /* options.c_cflag &= ~CRTSCTS; */     /* No hardware flow control - CRTSCTS not available on SGI */
    options.c_cflag |= CREAD | CLOCAL; /* Enable receiver, ignore modem control */
    
    /* Configure input settings */
    options.c_iflag &= ~(IXON | IXOFF | IXANY); /* No software flow control */
    options.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* Raw input */
    
    /* Configure output settings */
    options.c_oflag &= ~OPOST;       /* Raw output */
    
    /* Configure local settings */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* Raw mode */
    
    /* Set timeouts */
    options.c_cc[VMIN] = 0;          /* Minimum characters to read */
    options.c_cc[VTIME] = 10;        /* Timeout in deciseconds (1 second) */
    
    /* Apply settings */
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("tcsetattr");
        return -1;
    }
    
    /* Flush any existing data */
    tcflush(fd, TCIOFLUSH);
    
    return 0;
}

/* Send command to Fastrak */
int send_command(int fd, const char *cmd) {
    int len = strlen(cmd);
    int bytes_written = write(fd, cmd, len);
    
    /* printf("DEBUG: Sending command: [%s] (%d bytes)\n", cmd, len); */
    
    if (bytes_written != len) {
        perror("write");
        return -1;
    }
    
    /* Give device time to process command */
    usleep(100000); /* 100ms */
    
    /* Try to read any response */
    {
        char response[256];
        int response_bytes = read(fd, response, sizeof(response) - 1);
        if (response_bytes > 0) {
            response[response_bytes] = '\0';
            /* printf("DEBUG: Command response: [%s] (%d bytes)\n", response, response_bytes); */
        }
    }
    
    return 0;
}

/* Parse binary format Fastrak data */
int parse_fastrak_binary(const unsigned char *buffer, int len, FastrakData *data) {
    /* Fastrak binary format is typically 12 bytes per record:
     * 2 bytes: X position
     * 2 bytes: Y position  
     * 2 bytes: Z position
     * 2 bytes: Azimuth
     * 2 bytes: Elevation
     * 2 bytes: Roll
     */
    
    if (len < 12) return 0;
    
    /* Convert binary data to float values (simplified - actual conversion 
       depends on Fastrak scaling factors) */
    data->station = 1; /* Assume station 1 for now */
    
    /* Basic conversion - you may need to adjust scaling */
    data->x = (float)((short)(buffer[0] | (buffer[1] << 8))) / 100.0f;
    data->y = (float)((short)(buffer[2] | (buffer[3] << 8))) / 100.0f;
    data->z = (float)((short)(buffer[4] | (buffer[5] << 8))) / 100.0f;
    data->azimuth = (float)((short)(buffer[6] | (buffer[7] << 8))) / 100.0f;
    data->elevation = (float)((short)(buffer[8] | (buffer[9] << 8))) / 100.0f;
    data->roll = (float)((short)(buffer[10] | (buffer[11] << 8))) / 100.0f;
    
    data->valid = 1;
    return 1;
}

/* Parse ASCII format Fastrak data (keeping for reference) */
int parse_fastrak_ascii(const char *line, FastrakData *data) {
    /* Fastrak ASCII format: 01 -12.34 5.67 -8.90 123.45 -67.89 12.34 */
    if (sscanf(line, "%d %f %f %f %f %f %f",
               &data->station,
               &data->x, &data->y, &data->z,
               &data->azimuth, &data->elevation, &data->roll) == 7) {
        data->valid = 1;
        return 1;
    }
    return 0;
}

/* Display Fastrak data */
void display_data(const FastrakData *data) {
    if (!data->valid) return;
    
    if (use_opengl) {
        /* Update global data for OpenGL thread */
        pthread_mutex_lock(&data_mutex);
        current_data = *data;
        pthread_mutex_unlock(&data_mutex);
    } else {
        printf("Station %d: ", data->station);
        printf("Pos(%.2f, %.2f, %.2f) ", data->x, data->y, data->z);
        printf("Orient(Az:%.1f El:%.1f Ro:%.1f)\n", 
               data->azimuth, data->elevation, data->roll);
    }
}

/* Draw a colored cube */
void draw_cube() {
    glBegin(GL_QUADS);
    
    /* Front face (red) */
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    
    /* Back face (green) */
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    
    /* Top face (blue) */
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    
    /* Bottom face (yellow) */
    glColor3f(1.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    
    /* Right face (magenta) */
    glColor3f(1.0f, 0.0f, 1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    
    /* Left face (cyan) */
    glColor3f(0.0f, 1.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    
    glEnd();
}

/* Render the OpenGL scene */
void render_scene() {
    FastrakData data;
    static int frame_count = 0;
    
    /* Get current Fastrak data */
    pthread_mutex_lock(&data_mutex);
    data = current_data;
    pthread_mutex_unlock(&data_mutex);
    
    /* Clear screen */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    /* Set camera position - much further back to see large translations */
    gluLookAt(0.0, 0.0, 50.0,  /* Camera position - moved back */
              0.0, 0.0, 0.0,   /* Look at origin */
              0.0, 1.0, 0.0);  /* Up vector */
    
    /* Debug removed - using targeted debug in movement section */
    
    /* Add new data to history buffer */
    if (data.valid) {
        static float min_x = 1000.0f, max_x = -1000.0f;
        static int sample_count = 0;
        
        /* Track min/max for auto-scaling */
        if (data.x < min_x) min_x = data.x;
        if (data.x > max_x) max_x = data.x;
        
        /* Add to history buffer */
        x_history[history_index] = data.x;
        history_index = (history_index + 1) % GRAPH_HISTORY;
        if (history_count < GRAPH_HISTORY) history_count++;
        
        /* Show debug info */
        if (++sample_count % 30 == 0) { /* Every half second */
            printf("Raw X: %.2f  Range: [%.1f, %.1f]  Samples: %d\n",
                   data.x, min_x, max_x, sample_count);
        }
    }
    
    /* Set up 2D orthographic projection for graph */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    /* Disable depth testing for 2D drawing */
    glDisable(GL_DEPTH_TEST);
    
    /* Draw graph background */
    glColor3f(0.1f, 0.1f, 0.2f);
    glBegin(GL_QUADS);
        glVertex2f(50, 50);
        glVertex2f(window_width-50, 50);
        glVertex2f(window_width-50, window_height-50);
        glVertex2f(50, window_height-50);
    glEnd();
    
    /* Draw graph border */
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(50, 50);
        glVertex2f(window_width-50, 50);
        glVertex2f(window_width-50, window_height-50);
        glVertex2f(50, window_height-50);
    glEnd();
    
    /* Draw center line */
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
        glVertex2f(50, window_height/2);
        glVertex2f(window_width-50, window_height/2);
    glEnd();
    
    /* Draw the data line if we have data */
    if (history_count > 1 && data.valid) {
        static float min_val = 1000.0f, max_val = -1000.0f;
        float range_val;
        int i, start_idx;
        float x_step;
        
        /* Find min/max in current history for scaling */
        min_val = max_val = x_history[0];
        for (i = 0; i < history_count; i++) {
            if (x_history[i] < min_val) min_val = x_history[i];
            if (x_history[i] > max_val) max_val = x_history[i];
        }
        
        range_val = max_val - min_val;
        if (range_val < 1.0f) range_val = 1.0f; /* Avoid division by zero */
        
        x_step = (float)(window_width - 100) / (float)(history_count - 1);
        start_idx = (history_index - history_count + GRAPH_HISTORY) % GRAPH_HISTORY;
        
        /* Draw the line */
        glColor3f(1.0f, 0.0f, 0.0f); /* Red line */
        glBegin(GL_LINE_STRIP);
        for (i = 0; i < history_count; i++) {
            int idx = (start_idx + i) % GRAPH_HISTORY;
            float y = 50 + ((x_history[idx] - min_val) / range_val) * (window_height - 100);
            float x = 50 + i * x_step;
            glVertex2f(x, y);
        }
        glEnd();
        
        /* Draw current value as a circle */
        if (history_count > 0) {
            int current_idx = (history_index - 1 + GRAPH_HISTORY) % GRAPH_HISTORY;
            float y = 50 + ((x_history[current_idx] - min_val) / range_val) * (window_height - 100);
            float x = 50 + (history_count - 1) * x_step;
            
            glColor3f(1.0f, 1.0f, 0.0f); /* Yellow dot */
            glPointSize(5.0f);
            glBegin(GL_POINTS);
            glVertex2f(x, y);
            glEnd();
        }
    }
    
    /* Draw text info */
    if (data.valid && history_count > 0) {
        int current_idx = (history_index - 1 + GRAPH_HISTORY) % GRAPH_HISTORY;
        /* Text is complex without font support, just show in console */
        if (++frame_count % 60 == 0) {
            printf("Current: %.2f  History: %d samples\n", 
                   x_history[current_idx], history_count);
        }
    }
    
    /* Restore 3D projection */
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    /* Force completion and swap/flush */
    glFlush();
    glXSwapBuffers(display, window);
}

/* Setup OpenGL viewport and projection */
void setup_viewport(int width, int height) {
    window_width = width;
    window_height = height;
    
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
}

/* Initialize X11 and OpenGL */
int init_x11_opengl() {
    int screen;
    Window root;
    XVisualInfo *visual;
    Colormap colormap;
    XSetWindowAttributes window_attrs;
    /* Lighting variables removed - not using lighting for now */
    
    /* Open X display */
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        return -1;
    }
    
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    
    /* Try different visual configurations for SGI compatibility */
    visual = NULL;
    
    /* First try: Simple RGBA with double buffer */
    {
        int visual_attrs1[] = {
            GLX_RGBA,
            GLX_DOUBLEBUFFER,
            None
        };
        visual = glXChooseVisual(display, screen, visual_attrs1);
    }
    
    /* Second try: Add depth buffer */
    if (!visual) {
        int visual_attrs2[] = {
            GLX_RGBA,
            GLX_DEPTH_SIZE, 16,
            GLX_DOUBLEBUFFER,
            None
        };
        visual = glXChooseVisual(display, screen, visual_attrs2);
    }
    
    /* Third try: Single buffer fallback */
    if (!visual) {
        int visual_attrs3[] = {
            GLX_RGBA,
            None
        };
        visual = glXChooseVisual(display, screen, visual_attrs3);
    }
    
    if (!visual) {
        fprintf(stderr, "Cannot find suitable visual\n");
        XCloseDisplay(display);
        return -1;
    }
    
    printf("Using visual: depth=%d, class=%d\n", visual->depth, visual->class);
    
    /* Create colormap - use default if possible */
    if (visual->visual == DefaultVisual(display, screen)) {
        colormap = DefaultColormap(display, screen);
    } else {
        colormap = XCreateColormap(display, root, visual->visual, AllocNone);
    }
    
    /* Set window attributes */
    window_attrs.colormap = colormap;
    window_attrs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
    window_attrs.background_pixel = 0;
    window_attrs.border_pixel = 0;
    
    /* Create window */
    window = XCreateWindow(display, root, 100, 100, 
                          window_width, window_height, 0,
                          visual->depth, InputOutput, visual->visual,
                          CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, 
                          &window_attrs);
    
    if (!window) {
        fprintf(stderr, "Cannot create window\n");
        XCloseDisplay(display);
        return -1;
    }
    
    XStoreName(display, window, "Fastrak 3SP - OpenGL Cube Visualization");
    XMapWindow(display, window);
    XFlush(display);
    
    /* Create OpenGL context */
    context = glXCreateContext(display, visual, NULL, GL_TRUE);
    if (!context) {
        fprintf(stderr, "Cannot create OpenGL context\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return -1;
    }
    
    if (!glXMakeCurrent(display, window, context)) {
        fprintf(stderr, "Cannot make OpenGL context current\n");
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return -1;
    }
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    
    /* Set up OpenGL state - simplified for compatibility */
    glEnable(GL_DEPTH_TEST);
    
    /* Disable lighting initially for simpler debugging */
    glDisable(GL_LIGHTING);
    
    /* Background color */
    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    
    /* OpenGL setup complete */
    printf("OpenGL setup complete.\n");
    
    /* Initial viewport setup */
    setup_viewport(window_width, window_height);
    
    return 0;
}

/* OpenGL event loop */
void opengl_event_loop() {
    XEvent event;
    KeySym key;
    
    printf("OpenGL window created. Press ESC or Q to quit.\n");
    
    while (running) {
        /* Handle X11 events */
        while (XPending(display)) {
            XNextEvent(display, &event);
            
            switch (event.type) {
                case Expose:
                    render_scene();
                    break;
                    
                case ConfigureNotify:
                    setup_viewport(event.xconfigure.width, event.xconfigure.height);
                    break;
                    
                case KeyPress:
                    key = XLookupKeysym(&event.xkey, 0);
                    if (key == XK_Escape || key == XK_q || key == XK_Q) {
                        running = 0;
                    }
                    break;
            }
        }
        
        /* Render frame */
        render_scene();
        
        /* Small delay to prevent 100% CPU usage */
        usleep(8000); /* ~120 FPS */
    }
    
    /* Cleanup */
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

/* Initialize Fastrak device */
int initialize_fastrak(int fd) {
    printf("Initializing Fastrak 3SP...\n");
    
    /* Reset device to known state */
    if (send_command(fd, CMD_RESET) < 0) {
        fprintf(stderr, "Failed to send reset command\n");
        return -1;
    }
    
    sleep(2); /* Wait for reset to complete */
    
    /* Set binary output format (device seems to prefer this) */
    if (send_command(fd, CMD_BINARY) < 0) {
        fprintf(stderr, "Failed to set binary format\n");
        return -1;
    }
    
    /* Enable station 1 */
    if (send_command(fd, CMD_ENABLE_STATION) < 0) {
        fprintf(stderr, "Failed to enable station\n");
        return -1;
    }
    
    /* Configure output list for station 1 */
    if (send_command(fd, CMD_OUTPUT_LIST) < 0) {
        fprintf(stderr, "Failed to set output list\n");
        return -1;
    }
    
    /* Start continuous output */
    if (send_command(fd, CMD_CONTINUOUS) < 0) {
        fprintf(stderr, "Failed to start continuous mode\n");
        return -1;
    }
    
    printf("Fastrak initialized successfully\n");
    return 0;
}

/* Read and process Fastrak data */
void read_fastrak_data(int fd) {
    char buffer[MAX_BUFFER];
    int bytes_read;
    FastrakData data;
    
    printf("Reading Fastrak data (Press Ctrl+C to stop)...\n");
    printf("Station | Position (X, Y, Z) | Orientation (Az, El, Ro)\n");
    printf("--------|-------------------|------------------------\n");
    
    while (running) {
        /* Read data from serial port */
        bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No data available, continue */
                usleep(1000); /* 1ms */
                continue;
            } else {
                perror("read");
                break;
            }
        }
        
        if (bytes_read == 0) {
            /* No data, continue */
            usleep(1000); /* 1ms - much faster polling */
            continue;
        }
        
        /* DEBUG: Show raw data received */
        buffer[bytes_read] = '\0';
        /* printf("DEBUG: Received %d bytes: [", bytes_read);
        {
            int i;
            for (i = 0; i < bytes_read; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) {
                    printf("%c", buffer[i]);
                } else {
                    printf("\\x%02x", (unsigned char)buffer[i]);
                }
            }
        }
        printf("]\n"); */
        
        /* Process received data as binary */
        if (bytes_read >= 12) {
            /* Try to parse binary data every 12 bytes */
            int i;
            for (i = 0; i <= bytes_read - 12; i++) {
                memset(&data, 0, sizeof(data));
                if (parse_fastrak_binary((unsigned char*)(buffer + i), 12, &data)) {
                    display_data(&data);
                    i += 11; /* Skip ahead (loop will add 1) */
                }
            }
        }
    }
}

/* Thread function for reading Fastrak data */
void* fastrak_thread(void* arg) {
    int fd = *(int*)arg;
    read_fastrak_data(fd);
    return NULL;
}

/* Print usage information */
void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -d <device>    Serial device (default: %s)\n", DEFAULT_DEVICE);
    printf("  -p             Point mode (single reading instead of continuous)\n");
    printf("  -g             OpenGL mode (3D cube visualization)\n");
    printf("  -h             Show this help\n");
    printf("\nExamples:\n");
    printf("  %s -d /dev/ttyd1          # Console output\n", program_name);
    printf("  %s -d /dev/ttyd1 -g       # OpenGL visualization\n", program_name);
}

int main(int argc, char *argv[]) {
    char *device = DEFAULT_DEVICE;
    int point_mode = 0;
    int opt;
    
    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "d:pgh")) != -1) {
        switch (opt) {
            case 'd':
                device = optarg;
                break;
            case 'p':
                point_mode = 1;
                break;
            case 'g':
                use_opengl = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Install signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Fastrak 3SP Reader for SGI IRIX\n");
    printf("Connecting to device: %s\n", device);
    
    /* Open serial port */
    serial_fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", device, strerror(errno));
        fprintf(stderr, "Common SGI serial ports: /dev/ttyf1, /dev/ttyf2, /dev/ttyd1, /dev/ttyd2\n");
        return 1;
    }
    
    /* Configure serial port */
    if (configure_serial_port(serial_fd) < 0) {
        fprintf(stderr, "Failed to configure serial port\n");
        close(serial_fd);
        return 1;
    }
    
    /* Initialize Fastrak device */
    if (initialize_fastrak(serial_fd) < 0) {
        fprintf(stderr, "Failed to initialize Fastrak device\n");
        close(serial_fd);
        return 1;
    }
    
    if (use_opengl) {
        /* OpenGL visualization mode */
        printf("Starting OpenGL visualization...\n");
        printf("Controls: ESC or Q to quit\n");
        
        /* Initialize X11/OpenGL */
        if (init_x11_opengl() < 0) {
            fprintf(stderr, "Failed to initialize OpenGL\n");
            close(serial_fd);
            return 1;
        }
        
        /* Start Fastrak reading thread */
        {
            pthread_t fastrak_tid;
            if (pthread_create(&fastrak_tid, NULL, fastrak_thread, &serial_fd) != 0) {
                fprintf(stderr, "Failed to create Fastrak thread\n");
                close(serial_fd);
                return 1;
            }
        }
        
        /* Start OpenGL event loop */
        opengl_event_loop();
        
    } else if (point_mode) {
        /* Single point reading */
        FastrakData data;
        char buffer[MAX_BUFFER];
        int bytes_read;
        
        printf("Taking single reading...\n");
        send_command(serial_fd, CMD_POINT);
        
        /* Wait for response */
        sleep(1);
        bytes_read = read(serial_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            if (bytes_read >= 12) {
                if (parse_fastrak_binary((unsigned char*)buffer, 12, &data)) {
                    display_data(&data);
                } else {
                    printf("Failed to parse binary data\n");
                }
            } else {
                printf("Insufficient data received (%d bytes)\n", bytes_read);
            }
        }
    } else {
        /* Console continuous reading */
        read_fastrak_data(serial_fd);
    }
    
    /* Cleanup */
    if (serial_fd >= 0) {
        close(serial_fd);
    }
    
    printf("Fastrak reader stopped\n");
    return 0;
}
