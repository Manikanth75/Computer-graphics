// Smart Interactive 3D Analog Clock with Lighting (OpenGL + FreeGLUT, MinGW / Windows)
// Compile: g++ clock_lighting.cpp -o clock_lighting -lfreeglut -lopengl32 -lglu32
//
// FEATURES
//   1. Real-time analog clock (system time)
//   2. Smooth-moving second hand
//   3. Arabic numerals (1-12) + 60 tick marks
//   4. Digital time + date/day
//   5. Multiple clock themes
//   6. Alarm + stopwatch + countdown modes
//   7. OpenGL lighting (movable coloured light, shininess)
//
// KEYBOARD
//   T          change theme
//   D          toggle digital display
//   A          alarm mode   (H = hour+1, + / - = minutes, Enter = arm/disarm)
//   S          stopwatch    (Space = start/pause, 0 = reset)
//   C          countdown    (+ / - = minutes, Space = start/pause, 0 = reset)
//   L          light ON / OFF
//   M          change light colour
//   R          reset view and light
//   Arrow keys rotate the clock
//   ESC        exit
//   (any key stops a ringing alarm)
// MOUSE
//   Left-drag rotate | Right-drag move the light | Wheel zoom

#define NOMINMAX
#include <windows.h>
#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>

const float PI = 3.14159265f;

// ---------------------------------------------------------------- state
float rotX = -15.0f, rotY = 0.0f, camDist = 4.5f;
float lightX = 2.0f, lightY = 2.0f, lightZ = 4.0f;
bool  lightOn = true;
int   lightColorMode = 0;
int   winW = 800, winH = 800;
GLUquadric *quad;

// ---------------------------------------------------------------- themes
struct Theme {
    const char *name;
    float bg[3], face[3], body[3], bezel[3];
    float tickMaj[3], tickMin[3], numeral[3];
    float hand[3], second[3], digital[3];
};

const Theme themes[] = {
    {"Classic",
        {0.08f, 0.08f, 0.12f}, {0.95f, 0.93f, 0.85f}, {0.20f, 0.20f, 0.25f}, {0.80f, 0.65f, 0.20f},
        {0.10f, 0.10f, 0.10f}, {0.35f, 0.35f, 0.35f}, {0.10f, 0.10f, 0.10f},
        {0.10f, 0.10f, 0.10f}, {0.90f, 0.10f, 0.10f}, {0.15f, 0.15f, 0.15f}},
    {"Midnight",
        {0.02f, 0.02f, 0.05f}, {0.08f, 0.09f, 0.14f}, {0.15f, 0.15f, 0.20f}, {0.75f, 0.75f, 0.80f},
        {0.90f, 0.90f, 0.95f}, {0.50f, 0.50f, 0.60f}, {0.90f, 0.90f, 0.95f},
        {0.95f, 0.95f, 1.00f}, {1.00f, 0.30f, 0.20f}, {0.40f, 0.90f, 1.00f}},
    {"Ocean",
        {0.03f, 0.10f, 0.15f}, {0.75f, 0.90f, 0.95f}, {0.10f, 0.30f, 0.40f}, {0.90f, 0.90f, 0.95f},
        {0.05f, 0.20f, 0.30f}, {0.20f, 0.40f, 0.50f}, {0.05f, 0.20f, 0.30f},
        {0.05f, 0.20f, 0.30f}, {1.00f, 0.45f, 0.10f}, {0.05f, 0.25f, 0.35f}},
    {"Neon",
        {0.05f, 0.05f, 0.05f}, {0.05f, 0.12f, 0.06f}, {0.10f, 0.10f, 0.10f}, {0.20f, 0.90f, 0.40f},
        {0.30f, 1.00f, 0.50f}, {0.15f, 0.50f, 0.25f}, {0.30f, 1.00f, 0.50f},
        {0.40f, 1.00f, 0.60f}, {1.00f, 0.20f, 0.80f}, {0.30f, 1.00f, 0.50f}}
};
const int NUM_THEMES = 4;
int  themeIdx = 0;
bool showDigital = true;

// ---------------------------------------------------------------- modes
enum Mode { MODE_CLOCK, MODE_ALARM, MODE_STOPWATCH, MODE_COUNTDOWN };
Mode mode = MODE_CLOCK;

// alarm
int  alarmH = 7, alarmM = 30;
bool alarmArmed = false, alarmFired = false;
// stopwatch (milliseconds, based on GLUT_ELAPSED_TIME)
bool swRunning = false;
int  swStart = 0, swAccum = 0;
// countdown
int  cdMinutes = 1, cdRemaining = 60000, cdEnd = 0;
bool cdRunning = false;
// ringing
bool ringing = false;
char ringMsg[64] = "";
int  lastBeep = 0;

const char *DAYS[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

const GLfloat lightCols[3][4] = {
    {1.0f, 1.0f, 1.0f, 1.0f},    // white
    {1.0f, 0.85f, 0.6f, 1.0f},   // warm
    {0.6f, 0.8f, 1.0f, 1.0f}     // cool
};

// ---------------------------------------------------------------- helpers
int swElapsed() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    return swAccum + (swRunning ? now - swStart : 0);
}

void startRing(const char *msg) {
    ringing = true;
    strncpy(ringMsg, msg, sizeof(ringMsg) - 1);
    ringMsg[sizeof(ringMsg) - 1] = '\0';
    lastBeep = -1000;
}

void setupLight() {
    GLfloat pos[]  = {lightX, lightY, lightZ, 1.0f};   // positional light
    GLfloat amb[]  = {0.25f, 0.25f, 0.25f, 1.0f};
    GLfloat spec[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightCols[lightColorMode]);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
}

void box(float w, float h, float d) {
    glPushMatrix();
    glScalef(w, h, d);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Text drawn with stroke font on the clock face (rotates with the clock)
void strokeText(const char *s, float h, float x, float y, float z,
                const float *col, float lineWidth) {
    float sc = h / 119.05f;                      // stroke font cap height = 119.05
    float w  = glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char *)s) * sc;
    glDisable(GL_LIGHTING);
    glColor3fv(col);
    glLineWidth(lineWidth);
    glPushMatrix();
    glTranslatef(x - w / 2.0f, y - h / 2.0f, z);
    glScalef(sc, sc, sc);
    for (const char *p = s; *p; p++)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, (unsigned char)*p);
    glPopMatrix();
    if (lightOn) glEnable(GL_LIGHTING);
}

void drawHand(float angleDeg, float length, float width, float z, const float *col) {
    glPushMatrix();
    glTranslatef(0, 0, z);
    glRotatef(-angleDeg, 0, 0, 1);               // clockwise from 12 o'clock
    glColor3fv(col);
    glTranslatef(0, length / 2.0f - 0.05f, 0);
    box(width, length, 0.02f);
    glPopMatrix();
}

// ---------------------------------------------------------------- clock parts
void drawClockBody(const Theme &th) {
    // metal body (cylinder from z = -0.1 to 0.1)
    glColor3fv(th.body);
    glPushMatrix();
    glTranslatef(0, 0, -0.1f);
    gluCylinder(quad, 1.0, 1.0, 0.2, 64, 1);
    glRotatef(180, 0, 1, 0);                     // back cover
    gluDisk(quad, 0, 1.0, 64, 1);
    glPopMatrix();

    // clock face
    glColor3fv(th.face);
    glPushMatrix();
    glTranslatef(0, 0, 0.1f);
    gluDisk(quad, 0, 1.0, 64, 1);
    glPopMatrix();

    // bezel
    glColor3fv(th.bezel);
    glPushMatrix();
    glTranslatef(0, 0, 0.1f);
    glutSolidTorus(0.08, 1.0, 20, 64);
    glPopMatrix();
}

void drawTicks(const Theme &th) {
    for (int i = 0; i < 60; i++) {
        glPushMatrix();
        glTranslatef(0, 0, 0.11f);
        glRotatef(-i * 6.0f, 0, 0, 1);
        if (i % 5 == 0) {
            glColor3fv(th.tickMaj);
            glTranslatef(0, 0.84f, 0);
            box(0.04f, 0.12f, 0.03f);
        } else {
            glColor3fv(th.tickMin);
            glTranslatef(0, 0.87f, 0);
            box(0.015f, 0.05f, 0.03f);
        }
        glPopMatrix();
    }
}

void drawNumerals(const Theme &th) {
    char buf[8];
    for (int n = 1; n <= 12; n++) {
        float a = n * 30.0f * PI / 180.0f;
        snprintf(buf, sizeof(buf), "%d", n);
        strokeText(buf, 0.11f, 0.64f * sinf(a), 0.64f * cosf(a), 0.112f, th.numeral, 2.0f);
    }
}

void buildDisplayText(const SYSTEMTIME &st, char *l1, char *l2) {
    switch (mode) {
        case MODE_CLOCK:
            snprintf(l1, 32, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
            snprintf(l2, 48, "%s %02d %s %d", DAYS[st.wDayOfWeek], st.wDay,
                     MONTHS[st.wMonth - 1], st.wYear);
            break;
        case MODE_ALARM:
            snprintf(l1, 32, "%02d:%02d", alarmH, alarmM);
            snprintf(l2, 48, "%s", alarmArmed ? "ALARM ON" : "ALARM OFF");
            break;
        case MODE_STOPWATCH: {
            int ms = swElapsed();
            snprintf(l1, 32, "%02d:%02d.%02d", (ms / 60000) % 100, (ms / 1000) % 60, (ms / 10) % 100);
            snprintf(l2, 48, "STOPWATCH");
            break;
        }
        case MODE_COUNTDOWN: {
            int s = (cdRemaining + 999) / 1000;
            snprintf(l1, 32, "%02d:%02d", s / 60, s % 60);
            snprintf(l2, 48, "COUNTDOWN");
            break;
        }
    }
}

void drawDigital(const Theme &th, const SYSTEMTIME &st) {
    char l1[32], l2[48];
    buildDisplayText(st, l1, l2);
    strokeText(l1, 0.12f, 0.0f, -0.30f, 0.112f, th.digital, 2.5f);
    strokeText(l2, 0.06f, 0.0f, -0.45f, 0.112f, th.digital, 1.5f);
}

void drawAlarmMarker() {
    if (!alarmArmed) return;
    float angle = (alarmH % 12 + alarmM / 60.0f) * 30.0f;
    glPushMatrix();
    glTranslatef(0, 0, 0.18f);
    glRotatef(-angle, 0, 0, 1);
    glTranslatef(0, 1.0f, 0);
    glColor3f(1.0f, 0.1f, 0.1f);
    glutSolidSphere(0.045, 16, 16);
    glPopMatrix();
}

void drawLightMarker() {
    if (!lightOn) return;
    glDisable(GL_LIGHTING);
    glColor3fv(lightCols[lightColorMode]);
    glPushMatrix();
    glTranslatef(lightX, lightY, lightZ);
    glutSolidSphere(0.06, 16, 16);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------- HUD (2D overlay)
void beginHUD() {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}

void endHUD() {
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    if (lightOn) glEnable(GL_LIGHTING);
}

void drawText2D(float x, float y, void *font, const char *s) {
    glRasterPos2f(x, y);
    glutBitmapString(font, (const unsigned char *)s);
}

void drawHUD(const Theme &th) {
    static const char *modeName[] = {"CLOCK", "ALARM SET", "STOPWATCH", "COUNTDOWN"};
    char buf[200];

    beginHUD();
    glColor3f(0.92f, 0.92f, 0.92f);

    snprintf(buf, sizeof(buf), "Theme: %s    Light: %s    Mode: %s",
             th.name, lightOn ? "ON" : "OFF", modeName[mode]);
    drawText2D(12, winH - 22, GLUT_BITMAP_HELVETICA_18, buf);

    if (alarmArmed) {
        snprintf(buf, sizeof(buf), "Alarm armed for %02d:%02d", alarmH, alarmM);
        drawText2D(12, winH - 44, GLUT_BITMAP_HELVETICA_12, buf);
    }

    if (ringing) {
        glColor3f(1.0f, 1.0f, 0.2f);
        snprintf(buf, sizeof(buf), "*** %s ***  (press any key to stop)", ringMsg);
        drawText2D(12, winH - 76, GLUT_BITMAP_TIMES_ROMAN_24, buf);
        glColor3f(0.92f, 0.92f, 0.92f);
    }

    // mode-specific line
    switch (mode) {
        case MODE_CLOCK:
            snprintf(buf, sizeof(buf),
                     "Left-drag: rotate   Right-drag: move light   Wheel: zoom   Arrows: rotate");
            break;
        case MODE_ALARM:
            snprintf(buf, sizeof(buf),
                     "Alarm %02d:%02d [%s]  |  H: hour+1   +/-: minutes   Enter: arm/disarm",
                     alarmH, alarmM, alarmArmed ? "ARMED" : "OFF");
            break;
        case MODE_STOPWATCH: {
            int ms = swElapsed();
            snprintf(buf, sizeof(buf),
                     "Stopwatch %02d:%02d.%02d [%s]  |  Space: start/pause   0: reset",
                     (ms / 60000) % 100, (ms / 1000) % 60, (ms / 10) % 100,
                     swRunning ? "RUNNING" : "STOPPED");
            break;
        }
        case MODE_COUNTDOWN: {
            int s = (cdRemaining + 999) / 1000;
            snprintf(buf, sizeof(buf),
                     "Countdown %02d:%02d [%s]  |  +/-: set minutes   Space: start/pause   0: reset",
                     s / 60, s % 60, cdRunning ? "RUNNING" : "STOPPED");
            break;
        }
    }
    drawText2D(12, 62, GLUT_BITMAP_HELVETICA_12, buf);
    drawText2D(12, 40, GLUT_BITMAP_HELVETICA_12,
               "T: theme   D: digital display   L: light on/off   M: light colour   R: reset view");
    drawText2D(12, 18, GLUT_BITMAP_HELVETICA_12,
               "A: alarm   S: stopwatch   C: countdown   Esc: exit");

    endHUD();
}

// ---------------------------------------------------------------- display
void display() {
    const Theme &th = themes[themeIdx];
    int now = glutGet(GLUT_ELAPSED_TIME);
    SYSTEMTIME st;
    GetLocalTime(&st);

    if (ringing && (now / 250) % 2 == 0) glClearColor(0.6f, 0.05f, 0.05f, 1.0f);   // flash
    else glClearColor(th.bg[0], th.bg[1], th.bg[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, camDist, 0, 0, 0, 0, 1, 0);

    // light is set in world space, so the clock rotates under it
    if (lightOn) { glEnable(GL_LIGHTING); setupLight(); }
    else         { glDisable(GL_LIGHTING); }

    drawLightMarker();

    glRotatef(rotX, 1, 0, 0);
    glRotatef(rotY, 0, 1, 0);

    drawClockBody(th);
    drawTicks(th);
    drawNumerals(th);
    if (showDigital) drawDigital(th, st);
    drawAlarmMarker();

    // smooth hands (milliseconds included)
    float sec     = st.wSecond + st.wMilliseconds / 1000.0f;
    float minutes = st.wMinute + sec / 60.0f;
    float hours   = (st.wHour % 12) + minutes / 60.0f;

    drawHand(hours * 30.0f,   0.50f, 0.07f, 0.13f, th.hand);
    drawHand(minutes * 6.0f,  0.75f, 0.05f, 0.16f, th.hand);
    drawHand(sec * 6.0f,      0.85f, 0.02f, 0.19f, th.second);

    // centre cap
    glColor3fv(th.bezel);
    glPushMatrix();
    glTranslatef(0, 0, 0.19f);
    glutSolidSphere(0.05, 24, 24);
    glPopMatrix();

    drawHUD(th);
    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / h, 0.1, 50.0);
    glMatrixMode(GL_MODELVIEW);
}

// ---------------------------------------------------------------- update
void update() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    SYSTEMTIME st;
    GetLocalTime(&st);

    bool match = alarmArmed && st.wHour == alarmH && st.wMinute == alarmM;
    if (match && !alarmFired) { alarmFired = true; startRing("ALARM!"); }
    if (!match) alarmFired = false;

    if (cdRunning) {
        cdRemaining = cdEnd - now;
        if (cdRemaining <= 0) {
            cdRemaining = 0;
            cdRunning = false;
            startRing("COUNTDOWN FINISHED!");
        }
    }

    if (ringing && now - lastBeep >= 700) {
        MessageBeep(MB_ICONEXCLAMATION);
        lastBeep = now;
    }
}

void timer(int) {
    update();
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

// ---------------------------------------------------------------- input
void adjustValue(int d) {
    if (mode == MODE_ALARM) {
        alarmM += d;
        if (alarmM > 59) { alarmM = 0;  alarmH = (alarmH + 1) % 24; }
        if (alarmM < 0)  { alarmM = 59; alarmH = (alarmH + 23) % 24; }
    } else if (mode == MODE_COUNTDOWN && !cdRunning) {
        cdMinutes += d;
        if (cdMinutes < 1)  cdMinutes = 1;
        if (cdMinutes > 99) cdMinutes = 99;
        cdRemaining = cdMinutes * 60000;
    }
}

void toggleRun() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (mode == MODE_STOPWATCH) {
        if (swRunning) { swAccum += now - swStart; swRunning = false; }
        else           { swStart = now; swRunning = true; }
    } else if (mode == MODE_COUNTDOWN) {
        if (cdRunning) { cdRunning = false; }
        else if (cdRemaining > 0) { cdEnd = now + cdRemaining; cdRunning = true; }
    }
}

void resetTimer() {
    if (mode == MODE_STOPWATCH) { swRunning = false; swAccum = 0; }
    else if (mode == MODE_COUNTDOWN) { cdRunning = false; cdRemaining = cdMinutes * 60000; }
}

void resetView() {
    rotX = -15; rotY = 0; camDist = 4.5f;
    lightX = 2; lightY = 2; lightZ = 4;
}

void keyboard(unsigned char key, int, int) {
    if (key == 27) exit(0);
    if (ringing) { ringing = false; glutPostRedisplay(); return; }   // any key stops the ring

    switch (tolower(key)) {
        case 't': themeIdx = (themeIdx + 1) % NUM_THEMES; break;
        case 'd': showDigital = !showDigital; break;
        case 'l': lightOn = !lightOn; break;
        case 'm': lightColorMode = (lightColorMode + 1) % 3; break;
        case 'a': mode = (mode == MODE_ALARM) ? MODE_CLOCK : MODE_ALARM; break;
        case 's': mode = (mode == MODE_STOPWATCH) ? MODE_CLOCK : MODE_STOPWATCH; break;
        case 'c': mode = (mode == MODE_COUNTDOWN) ? MODE_CLOCK : MODE_COUNTDOWN; break;
        case 'r': resetView(); break;
        case 'h': if (mode == MODE_ALARM) alarmH = (alarmH + 1) % 24; break;
        case '+': case '=': adjustValue(+1); break;
        case '-': case '_': adjustValue(-1); break;
        case 13:  if (mode == MODE_ALARM) alarmArmed = !alarmArmed; break;
        case ' ': toggleRun(); break;
        case '0': resetTimer(); break;
    }
    glutPostRedisplay();
}

void special(int key, int, int) {
    switch (key) {
        case GLUT_KEY_LEFT:  rotY -= 5; break;
        case GLUT_KEY_RIGHT: rotY += 5; break;
        case GLUT_KEY_UP:    rotX -= 5; break;
        case GLUT_KEY_DOWN:  rotX += 5; break;
    }
    if (rotX > 90)  rotX = 90;
    if (rotX < -90) rotX = -90;
    glutPostRedisplay();
}

int lastX = 0, lastY = 0, activeBtn = -1;

void mouse(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) { activeBtn = button; lastX = x; lastY = y; }
    else activeBtn = -1;
}

void motion(int x, int y) {
    int dx = x - lastX, dy = y - lastY;
    lastX = x; lastY = y;
    if (activeBtn == GLUT_LEFT_BUTTON) {
        rotY += dx * 0.5f;
        rotX += dy * 0.5f;
        if (rotX > 90)  rotX = 90;
        if (rotX < -90) rotX = -90;
    } else if (activeBtn == GLUT_RIGHT_BUTTON) {
        lightX += dx * 0.01f;
        lightY -= dy * 0.01f;
    }
    glutPostRedisplay();
}

void wheel(int, int dir, int, int) {
    camDist -= dir * 0.3f;
    if (camDist < 2.5f)  camDist = 2.5f;
    if (camDist > 10.0f) camDist = 10.0f;
    glutPostRedisplay();
}

// ---------------------------------------------------------------- init / main
void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);              // needed because of glScalef
    glShadeModel(GL_SMOOTH);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat spec[] = {0.6f, 0.6f, 0.6f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 60.0f);

    quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Smart Interactive Analog Clock Using OpenGL");
    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(wheel);
    glutTimerFunc(0, timer, 0);
    glutMainLoop();
    return 0;
}
