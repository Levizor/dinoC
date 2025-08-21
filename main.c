#include "string.h"
#include "sys/types.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define DINO_TEXTURE_WIDTH 21
#define DINO_TEXTURE_HEIGHT 18
#define CACTUS_TEXTURE_WIDTH 11
#define CACTUS_TEXTURE_HEIGHT 7

// clang-format off
static char *CACTUS_TEXTURE[] = {
  "  _  _     ",  
  " | || | _  ", 
  " | || || | ",
  "  \\_  || | ", 
  "    |  _/  ", 
  "    | |    ",
  "    |_|    "
};

static char *DINO_TEXTURE[] = {
    "           ######### ",
    "          ### #######",
    "          ###########",
    "          ###########",
    "          ######     ",
    "          #########  ",
    "#       #######      ",
    "##    ############   ",
    "###  ##########  #   ",
    "###############      ",
    "###############      ",
    " #############       ",
    "  ###########        ",
    "    ########         ",
    "     ###  ##         ",
    "     ##    #         ",
    "     #     #         ",
    "     ##    ##        "};
// clang-format on
//
static uint GROUND = 0;
static uint WIDTH = 0;
static uint HEIGHT = 0;
static uint INTERVAL = 30000;

// Game objects

typedef struct {
  ulong count;
  int x, y;
} Score;

typedef struct {
  int x;
  int y;
  unsigned int width;
  unsigned int height;
} Box;

int box_overlap(Box A, Box B) {

  if (A.x < B.x + B.width && A.x + A.width > B.x && A.y < B.y + B.height &&
      A.y + A.height > B.y) {
    return 1;
  }

  return 0;
}

typedef struct {
  char *map;
  int rows;
  int cols;
} Screen;

typedef struct {
  int x, y;
  double velocity;
  int onGround;
  Box cbox;
} Dino;

typedef struct {
  int x, y;
  int active;
  Box cbox;
} Obstacle;

typedef struct {
  Obstacle *self;
  size_t size;
} Obstacles;

void init_input() {
  struct termios newt;
  tcgetattr(STDIN_FILENO, &newt);
  newt.c_lflag &= ~(ICANON | ECHO); // disable canonical mode and echo
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // non-blocking stdin
}

int kbhit() {
  unsigned char ch;
  if (read(STDIN_FILENO, &ch, 1) == 1)
    return ch;
  return 0;
}

void clear_screen() {
  printf("\033[H\033[J"); // ANSI clear screen
}

void draw_texture(Screen *screen, int x, int y, int height, int width,
                  char *texture[]) {
  for (int i = 0; i < height; i++) {
    int sy = y + i;
    for (int j = 0; j < width; j++) {
      int sx = x + j;
      if (sy >= 0 && sy < screen->rows && sx >= 0 && sx < screen->cols)
        if (texture[i][j] != ' ') {
          screen->map[sy * screen->cols + sx] = texture[i][j];
        }
    }
  }
}

int can_spawn_new_obstacle(Obstacles *obstacles) {
  // find last obstacle
  Obstacle *last = obstacles->self;
  for (int i = 0; i < obstacles->size; i++) {
    if (last->x < obstacles->self[i].x) {
      last = &obstacles->self[i];
    }
  }

  // approximate distance
  if (WIDTH - last->x >= 30) {
    return 1;
  }
  return 0;
}

void draw_score(Screen *screen, Score *score) {
  char str[21 + 7];
  sprintf(str, "Score: %lu", score->count);
  memcpy(screen->map + screen->cols + (screen->cols - strlen(str)), str,
         strlen(str));
}

void draw(Dino dino, Obstacles obstacles, Screen screen, Score score) {

  for (int i = 0; i < screen.rows * screen.cols; i++) {
    screen.map[i] = ' ';
  }

  draw_score(&screen, &score);
  // ground
  for (int i = GROUND * screen.cols; i < GROUND * screen.cols + WIDTH; i++) {
    screen.map[i] = '-';
  }

  // obstacles
  for (int i = 0; i < obstacles.size; i++) {
    Obstacle *obstacle = &obstacles.self[i];
    if (obstacle->active)
      draw_texture(&screen, obstacle->x, obstacle->y, CACTUS_TEXTURE_HEIGHT,
                   CACTUS_TEXTURE_WIDTH, CACTUS_TEXTURE);
  }

  // dino
  draw_texture(&screen, dino.x, dino.y, DINO_TEXTURE_HEIGHT, DINO_TEXTURE_WIDTH,
               DINO_TEXTURE);

  // print
  clear_screen();

  for (int i = 0; i < screen.rows; i++) {
    for (int j = 1; j < screen.cols; j++) {
      putchar(screen.map[i * screen.cols + j]);
    }
    putchar('\n');
  }
}

Screen init_screen() {
  struct winsize w;
  ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
  return (Screen){malloc(w.ws_row * w.ws_col * sizeof(char)), w.ws_row,
                  w.ws_col};
}

int main() {
  Screen screen = init_screen();
  Score score = {0, screen.rows - 10, 0};

  GROUND = screen.rows - screen.rows * 0.1;
  HEIGHT = screen.rows;
  WIDTH = screen.cols;

  Dino dino = {5, GROUND - DINO_TEXTURE_HEIGHT, 0, 1};
  dino.cbox = (Box){dino.x, dino.y, 9, DINO_TEXTURE_HEIGHT - 2};

  Obstacles obstacles = {malloc(sizeof(Obstacle) * screen.cols / 15),
                         screen.cols / 15};

  for (int i = 0; i < obstacles.size; i++) {
    obstacles.self[i].active = 0;
  }

  srand(time(NULL));
  init_input();

  while (1) {
    // input
    int key = kbhit();
    switch (key) {
    case ' ':
      if (dino.onGround) {
        dino.velocity = -3.5;
        dino.onGround = 0;
      }
      break;
    case 'q':
      return 0;
    }
    // physics
    if (!dino.onGround) {
      dino.y += dino.velocity;
      dino.velocity += 0.5; // gravity
      if (dino.y >= GROUND - DINO_TEXTURE_HEIGHT) {
        dino.y = GROUND - DINO_TEXTURE_HEIGHT;
        dino.onGround = 1;
      }
      dino.cbox.y = dino.y;
    }

    // obstacles
    for (int i = 0; i < obstacles.size; i++) {
      Obstacle *obs = &obstacles.self[i];
      if (obs->active) {
        obs->x -= 2;
        obs->cbox.x -= 2;
        if (obs->x + CACTUS_TEXTURE_WIDTH < 0) {
          score.count++;
          obs->active = 0;
        }
      } else if (rand() % 200 == 0 && can_spawn_new_obstacle(&obstacles)) {
        obs->x = WIDTH - 1;
        obs->y = GROUND - CACTUS_TEXTURE_HEIGHT;
        obs->cbox = (Box){WIDTH - 1, GROUND - CACTUS_TEXTURE_HEIGHT + 1,
                          CACTUS_TEXTURE_WIDTH - 1, CACTUS_TEXTURE_HEIGHT - 1};
        obs->active = 1;
      }
    }

    // collision
    for (int i = 0; i < obstacles.size; i++) {
      Obstacle *obs = &obstacles.self[i];
      if (box_overlap(obs->cbox, dino.cbox)) {
        printf("Game Over!\n");
        printf("Your score is: %lu\n", score.count);
        printf("INTERVAL: %du\n", INTERVAL);
        return 0;
      }
    }

    // drawing
    draw(dino, obstacles, screen, score);

    if (score.count == 10) {
      INTERVAL = 25000;
    } else if (score.count == 20) {
      INTERVAL = 20000;
    } else if (score.count == 30) {
      INTERVAL = 15000;
    }

    usleep(INTERVAL);
  }
}
