#include "raylib.h"
#include "raymath.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define screenWidth 1920
#define screenHeight 1080
struct Triangle {
  Vector2 a;
  Vector2 b;
  Vector2 c;
};

// this and the above struct are just so I can center the resulting graphic on
// the window
void DrawStructTriangle(struct Triangle input) {
  struct Triangle temp = input;
  temp.a.x += screenWidth / 2.0;
  temp.a.y += screenHeight / 2.0;

  temp.b.x += screenWidth / 2.0;
  temp.b.y += screenHeight / 2.0;

  temp.c.x += screenWidth / 2.0;
  temp.c.y += screenHeight / 2.0;
  DrawTriangleLines(temp.a, temp.b, temp.c, BLACK);
}

void drawTriangleCentered(struct Triangle input) {
  struct Triangle temp = input;
  temp.a = Vector2Scale(temp.a, 80.0);
  temp.b = Vector2Scale(temp.b, 80.0);
  temp.c = Vector2Scale(temp.c, 80.0);
  temp.a = Vector2Add(temp.a, (Vector2){screenWidth / 4.0, 0});
  temp.b = Vector2Add(temp.b, (Vector2){screenWidth / 4.0, 0});
  temp.c = Vector2Add(temp.c, (Vector2){screenWidth / 4.0, 0});
  DrawTriangle(temp.c, temp.b, temp.a, RED);
  DrawTriangleLines(temp.a, temp.b, temp.c, BLACK);
  return;
}
void drawVectorCentered(Vector2 start, Vector2 end) {
  Vector2 a = start;
  Vector2 b = end;
  a = Vector2Scale(a, 80.0);
  b = Vector2Scale(b, 80.0);
  a = Vector2Add(a, (Vector2){screenWidth / 4.0, 0});
  b = Vector2Add(b, (Vector2){screenWidth / 4.0, 0});
  DrawLine(a.x, a.y, b.x, b.y, BLUE);
}

// wall struct from dungeon-shooter
struct Wall {
  // these use z instead of y because this is meant to be implemented in a 3d
  // engine
  float x1, z1;
  float x2, z2;
  Texture tx;
  int solid;
};

// sector struct from dungeon-shooter
struct Sector {
  float ceilingHt;
  float floorHt;
  Texture ceilingTx;
  Texture floorTx;
  int nWalls;
  struct Wall *walls;
  int nfloorTris;
  struct Triangle *triangles;
};

void drawSectorOutline(struct Sector input) {
  int i;
  for (i = 0; i < input.nWalls; i++)
    drawVectorCentered((Vector2){input.walls[i].x1, input.walls[i].z1},
                       (Vector2){input.walls[i].x2, input.walls[i].z2});
  return;
}

void drawSectorTris(struct Sector input) {
  int i;
  for (i = 0; i < input.nfloorTris; i++) {
    drawTriangleCentered(input.triangles[i]);
  }
  return;
}

enum slurpstate { WALLCOUNT, WALLDEF };

int is_comment_line(const char *line) {
  // Skip leading whitespace
  while (isspace((unsigned char)*line)) {
    line++;
  }
  return (*line == '#');
}

struct Sector slurpFromFile(char *input) {
  struct Sector result;
  char ch;
  enum slurpstate state = WALLCOUNT;
  int lineN = 0;
  int wallIdx = 0;
  char line[256];
  FILE *file = fopen(input, "r");
  // open file

  while (fgets(line, sizeof(line), file)) {
    lineN++;

    // Remove trailing newline
    line[strcspn(line, "\n")] = '\0';

    // Skip empty lines
    if (line[0] == '\0') {
      continue;
    }

    // Skip comment lines
    if (is_comment_line(line)) {
      continue;
    }

    // If we haven't read the wall count yet, read it
    if (state == WALLCOUNT) {
      if (sscanf(line, "%d", &result.nWalls) != 1) {
        fclose(file);
        return result;
      }

			printf("%d walls\n", result.nWalls);
      result.walls = malloc(sizeof(struct Wall) * result.nWalls);

      state = WALLDEF;
      continue;
    }

    // Parse wall coordinates
    if (state == WALLDEF) {

      if (sscanf(line, "%f %f %f %f %d", &result.walls[wallIdx].x1,
                 &result.walls[wallIdx].z1, &result.walls[wallIdx].x2,
                 &result.walls[wallIdx].z2,
                 &result.walls[wallIdx].solid) != 5) {
        fprintf(stderr, "Error parsing wall coordinates on line %d\n", lineN);
        fclose(file);
        return result;
      }

      wallIdx++;
    }
  }

  fclose(file);

  // skip comments "#"
  // print out the number of walls
  // print out wall n goes from x1,z1 to x2,x2

  return result;
}

struct iNode {
  struct iNode *previous;
  struct iNode *next;
  int n;
};
struct iQueue {
  struct iNode *front;
  struct iNode *back;
  int members;
};

void removeNode(struct iNode *input) {
  input->previous->next = input->next;
  input->next->previous = input->previous;
  free(input);
}

// removes the node with i in it
void removeiNode(struct iQueue *input, int target) {
  int i;
  struct iNode *currentNode = input->front;
  for (i = 0; i < input->members; i++) {
    if (currentNode->n == target) {

      if (currentNode == input->front)
        input->front = currentNode->next;
      if (currentNode == input->back)
        input->back = currentNode->previous;

      input->members--;
      removeNode(currentNode);
      return;
    }
    currentNode = currentNode->next;
  }
}

// generates a doubly linked list of integers
struct iQueue *initQueue(int input) {
  int i;
  struct iNode *currentNode;
  struct iQueue *result = malloc(sizeof(struct iQueue));
  result->front = malloc(sizeof(struct iNode));
  currentNode = result->front;
  for (i = 0; i < input - 1; i++) {
    currentNode->n = i;
    currentNode->next = malloc(sizeof(struct iNode));
    currentNode->next->previous = currentNode;
    currentNode = currentNode->next;
  }
  currentNode->n = input - 1;

  currentNode->next = result->front;
  result->back = currentNode;
  result->front->previous = result->back;
  result->members = input;

  return result;
}
int triangleHasPointInside(int a, int b, int c, struct Triangle tempTriangle,
                           struct iQueue *walliQueue, struct Sector *input) {
  struct iNode *currentNode = walliQueue->front;
  Vector2 currentPoint;
  do {
    if (!((currentNode->n == a) || (currentNode->n == b) ||
          (currentNode->n == c))) {
      currentPoint = (Vector2){input->walls[currentNode->n].x1,
                               input->walls[currentNode->n].z1};
      if (CheckCollisionPointTriangle(currentPoint, tempTriangle.a,
                                      tempTriangle.b, tempTriangle.c)) {
        printf("there was a point in the proposed triangle\n");
        return 1;
      }
    } else {
      printf("skipping node %d\n", currentNode->n);
    }
    currentNode = currentNode->next;
  } while (currentNode != walliQueue->front);
  return 0;
}

void triangulate(struct Sector *input) {
  int i, j = 0;
  Vector2 currentPoint, previousPoint, nextPoint;
  Vector2 currentToPrev, currentToNext;
  struct Triangle tempTriangle;
  input->nfloorTris = input->nWalls - 2;
  struct iQueue *walliQueue = initQueue(input->nWalls);
  input->triangles = malloc(sizeof(struct Triangle) * input->nfloorTris);
  struct iNode *currentNode = walliQueue->front;
  do {
    currentNode = currentNode->next;
  } while (currentNode != walliQueue->back);
  currentNode = currentNode->next;
  while (walliQueue->members > 3) {
    // wow what a mess of pointers!
    // this is grabbing the coordinates of the start point of walls, using
    // currentNode as an index
    currentPoint = (Vector2){input->walls[currentNode->n].x1,
                             input->walls[currentNode->n].z1};
    previousPoint = (Vector2){input->walls[currentNode->previous->n].x1,
                              input->walls[currentNode->previous->n].z1};
    nextPoint = (Vector2){input->walls[currentNode->next->n].x1,
                          input->walls[currentNode->next->n].z1};

    currentToPrev = Vector2Subtract(previousPoint, currentPoint);
    currentToNext = Vector2Subtract(nextPoint, currentPoint);
    if ((currentToPrev.x * currentToNext.y) -
            (currentToPrev.y * currentToNext.x) <
        0.0f) {
      printf("%d %d %d formed an obtuse angle, moving on\n",
             currentNode->previous->n, currentNode->n, currentNode->next->n);
    } else {
      // TODO put point-inside-triangle check here
      tempTriangle = (struct Triangle){currentPoint, previousPoint, nextPoint};
      if (!triangleHasPointInside(currentNode->n, currentNode->next->n,
                                  currentNode->previous->n, tempTriangle,
                                  walliQueue, input)) {
        input->triangles[j++] = tempTriangle;
        removeiNode(walliQueue, currentNode->n);
        currentNode = walliQueue->front;
				continue;
      }
    }
    currentNode = currentNode->next;
  }
  currentPoint = (Vector2){input->walls[currentNode->n].x1,
                           input->walls[currentNode->n].z1};
  previousPoint = (Vector2){input->walls[currentNode->previous->n].x1,
                            input->walls[currentNode->previous->n].z1};
  nextPoint = (Vector2){input->walls[currentNode->next->n].x1,
                        input->walls[currentNode->next->n].z1};
  input->triangles[j++] =
      (struct Triangle){currentPoint, previousPoint, nextPoint};

  if (walliQueue->members == 3) {
    free(currentNode->next->next);
    free(currentNode->next);
    free(currentNode);
  } else
    fprintf(stderr, "Triangulation algorithm failed, didn't reduce edges to 3 "
                    "before exiting.\n This will lead to a memory leak.");

  free(walliQueue);

  return;
}

// you can give this a txt file with a list of walls, it will sort them and
// display the polygon they form divided into triangles
int main(int argc, char **argv) {

  InitWindow(screenWidth, screenHeight,
             "raylib [polygon triangulation] example - ear clipping");
  SetTargetFPS(60);
  struct Sector sector1 = slurpFromFile(argv[1]);
  triangulate(&sector1);

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    BeginDrawing();

    ClearBackground(RAYWHITE);

    DrawText("triangulation example", 20, 20, 20, DARKGRAY);

    drawSectorOutline(sector1);
    drawSectorTris(sector1);

    DrawLine(18, 42, screenWidth - 18, 42, BLACK);
    EndDrawing();
  }
  CloseWindow(); // Close window and OpenGL context
  free(sector1.walls);

  return 0;
}
