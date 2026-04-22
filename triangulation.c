#include "raylib.h"
#include "raymath.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define screenWidth 1920
#define screenHeight 1080
// colors
#define GRUVBOX_BACKGROUND (Color){28, 28, 28, 0xFF}
#define GRUVBOX_TEXT (Color){0xeb, 0xdb, 0xbb, 0xFF}
#define GRUVBOX_BLUE (Color){0x45, 0x85, 0x88, 0xFF}
struct Triangle {
  Vector2 a;
  Vector2 b;
  Vector2 c;
};

enum WallType { PERIMETER, INTERIOR, BRIDGE };
// wall struct from dungeon-shooter
struct Wall {
  // these use z instead of y because this is meant to be implemented in a 3d
  // engine
  float x1, z1;
  float x2, z2;
  Texture tx;
  int solid;
  enum WallType type;
};

// sector struct from dungeon-shooter
struct Sector {
  float ceilingHt;
  float floorHt;
  Texture ceilingTx;
  Texture floorTx;
  int nWalls;
  struct Wall *walls;
  int nHoles;
  int *holeIndexes;
  int nfloorTris;
  struct Triangle *triangles;
};
void triangulate(struct Sector *input);

void drawSectorOutline(struct Sector input, Vector2 center,
                       Vector2 shapeCenter) {
  int i;
  int textX, textY;
  Vector2 a;
  Vector2 b;
  for (i = 0; i < input.nWalls; i++) {
    a = (Vector2){input.walls[i].x1, input.walls[i].z1};
    b = (Vector2){input.walls[i].x2, input.walls[i].z2};
    // textX = (a.x + (b.x * .90)) / 2;
    // textY = (a.y + b.y) / 2;
    textX = ((a.x + b.x)) / 2;
    textY = ((a.y + b.y)) / 2;
    DrawLine(a.x, a.y, b.x, b.y, GRUVBOX_BLUE);
    DrawText(TextFormat("%d", i), textX, textY, 12, GRUVBOX_TEXT);
  }
  return;
}

void drawSectorTris(struct Sector input, Vector2 screenCenter,
                    Vector2 shapeCenter) {
  int i;
  for (i = 0; i < input.nfloorTris; i++) {
    DrawTriangle(input.triangles[i].a, input.triangles[i].b,
                 input.triangles[i].c, BLUE);
    DrawTriangleLines(input.triangles[i].a, input.triangles[i].b,
                      input.triangles[i].c, BLACK);
  }
  return;
}

enum SlurpState { WALLCOUNT, WALLDEF };
enum SectorState { PERIMETERDEF, HOLEDEF };
enum BridgeState { PERIMETERTOHOLE, HOLETOHOLE, HOLETOPERIMETER };

int is_comment_line(const char *line) {
  // Skip leading whitespace
  while (isspace((unsigned char)*line)) {
    line++;
  }
  return (*line == '#');
}
int bridgeCrossesBridge(struct Wall *bridges, int nBridges, Vector2 start,
                        Vector2 end) {
  Vector2 collisionPoint;
  Vector2 bridgeStart, bridgeEnd;
  int i;
  for (i = 0; i < nBridges; i++) {
    bridgeStart = (Vector2){bridges[i].x1, bridges[i].z1};
    bridgeEnd = (Vector2){bridges[i].x2, bridges[i].z2};
    if (CheckCollisionLines(start, end, bridgeStart, bridgeEnd,
                            &collisionPoint)) {
      // its not a real collision if the collision point is the destination
      // if (Vector2Equals(collisionPoint, end) ||
      //    Vector2Equals(collisionPoint, start)) {
      //  continue;
      //}
      printf("collided with bridge at %.0f, %.0f\n", collisionPoint.x,
             collisionPoint.y);
      return 1;
    }
  }
  return 0;
}

int bridgeCrossesWall(struct Sector *input, Vector2 start, Vector2 end) {
  Vector2 collisionPoint;
  Vector2 wallStart, wallEnd;
  int i;
  for (i = 0; i < input->nWalls; i++) {
    wallStart = (Vector2){input->walls[i].x1, input->walls[i].z1};
    wallEnd = (Vector2){input->walls[i].x2, input->walls[i].z2};
    if (CheckCollisionLines(start, end, wallStart, wallEnd, &collisionPoint)) {
      if (Vector2Equals(collisionPoint, end) ||
          Vector2Equals(collisionPoint, start)) {
        continue;
      }
      printf("collided with wall at %.0f, %.0f\n", collisionPoint.x,
             collisionPoint.y);
      return 1;
    }
  }
  return 0;
}

void printSectorHoleIndexes(struct Sector *input) {
  int i;
  printf("this sector has hole(s) that start at the following index(es)\n");
  for (i = 0; i < input->nHoles; i++)
    printf("%d\n", input->holeIndexes[i]);
  return;
}

void addBridge(struct Wall *bridges, int bridgeIdx, Vector2 start,
               Vector2 end) {

  bridges[bridgeIdx].x1 = start.x;
  bridges[bridgeIdx].x2 = end.x;
  bridges[bridgeIdx].z1 = start.y;
  bridges[bridgeIdx].z2 = end.y;
  bridges[bridgeIdx].type = BRIDGE;
  return;
}

void reverseWall(struct Wall *input) {
  float temp;
  temp = input->x1;
  input->x1 = input->x2;
  input->x2 = temp;

  temp = input->z1;
  input->z1 = input->z2;
  input->z2 = temp;
  return;
}

bool recursiveBridgeAdd(struct Sector *input, int nHoles, int firstBridgeIdx,
                        int bridgeFromIdx, struct Wall *bridges,
                        int nUsedBridges) {
  int endPointMin = 0;
  int endPointMax = 0;
  int i;
  Vector2 start, end;
  enum BridgeState bridgeState;
  printf("recursion depth: %d\n", nUsedBridges);
  // determine limit of possible endpoints
  if (nUsedBridges == 0)
    bridgeState = PERIMETERTOHOLE;
  else if (nUsedBridges == nHoles)
    bridgeState = HOLETOPERIMETER;
  else if (nUsedBridges < nHoles)
    bridgeState = HOLETOHOLE;
  else
    return true;

  switch (bridgeState) {
  case (PERIMETERTOHOLE):
    endPointMin = input->holeIndexes[0];
    endPointMax = input->nWalls;
    firstBridgeIdx = bridgeFromIdx;
    break;

  case (HOLETOPERIMETER):
    endPointMin = 0;
    endPointMax = input->holeIndexes[0];

    break;
  default:
    printf("huh?\n");
  }
  start =
      (Vector2){input->walls[bridgeFromIdx].x2, input->walls[bridgeFromIdx].z2};
  for (i = endPointMin; i < endPointMax; i++) {
    if (i == firstBridgeIdx || i == bridgeFromIdx)
      continue;
    end = (Vector2){input->walls[i].x1, input->walls[i].z1};
    printf("trying to bridge from %d to %d\n", bridgeFromIdx, i);
    if (bridgeCrossesBridge(bridges, nUsedBridges, start, end) ||
        bridgeCrossesWall(input, start, end))
      continue;
    addBridge(bridges, nUsedBridges, start, end);
    printf("using bridge from %d to %d\n", bridgeFromIdx, i);
    if (recursiveBridgeAdd(input, nHoles, firstBridgeIdx, i + 1, bridges,
                           nUsedBridges + 1))
      return true;
  }
  return false;
}

void addBridgesToSector(struct Sector *input, int nHoles) {
  int i;
  bool foundPath = false;
  enum BridgeState bridgeState = PERIMETERTOHOLE;
  struct Wall *bridges = malloc(sizeof(struct Wall) * (nHoles + 1));
  for (i = 0; i < input->nWalls; i++) {
    printf("trying to start bridges at %d\n", i);
    if (recursiveBridgeAdd(input, nHoles, -1, i, bridges, 0)) {
      foundPath = true;
      break;
    }
  }
  if (!foundPath) {
    printf("Error, tried all bridge paths and found none!\n");
    return;
  }
  input->nWalls += (2 * nHoles);
  input->walls = realloc(
      input->walls, sizeof(struct Wall) * (input->nWalls + (2 * (nHoles + 1))));
  for (i = 0; i < nHoles + 1; i++) {
    input->walls[input->nWalls] = bridges[i];
    input->nWalls++;
    reverseWall(&bridges[i]);
    input->walls[input->nWalls] = bridges[i];
    input->nWalls++;
  }
}
struct Sector *splitWork(struct Sector *input) {
  int i, j, k, wallIdx;
  bool *checkList = calloc(input->nWalls, sizeof(bool));
  struct Sector *result = malloc(sizeof(struct Sector) * 2);
  // deliberately over-allocate the walls for the splits
  // this doesn't matter because it gets freed after
  result[0].walls = malloc(sizeof(struct Wall) * input->nWalls);
  result[1].walls = malloc(sizeof(struct Wall) * input->nWalls);
  for (i = 0; i < input->nWalls; i++) {
		if(input->walls[i].type == INTERIOR)
			reverseWall(&input->walls[i]);
	}
  for (i = 0; i < 2; i++) {
    for (j = 0; j < input->nWalls; j++) {
      if (checkList[j])
        continue;
      result[i].walls[0] = input->walls[j];
      printf("starting with wall %d split %d\n", j, i);
      checkList[j] = true;
      break;
    }
    wallIdx = 0;

    while (result[i].walls[0].x1 != result[i].walls[wallIdx].x2 ||
           result[i].walls[0].z1 != result[i].walls[wallIdx].z2) {
      for (j = 0; j < input->nWalls; j++) {
        if (checkList[j])
          continue;
        if (input->walls[j].x1 == result[i].walls[wallIdx].x2 &&
            input->walls[j].z1 == result[i].walls[wallIdx].z2) {
          if (!(input->walls[j].type == BRIDGE &&
                result[i].walls[wallIdx].type == BRIDGE))
            k = j;
        }
      }
      wallIdx++;
      result[i].walls[wallIdx] = input->walls[k];
      printf("adding wall %d to split %d\n", k, i);
      checkList[k] = true;
    }
    result[i].nWalls = wallIdx + 1;
  }
  free(checkList);

  return result;
}

float edgeSum(struct Sector *input){
	int i;
	float sum = 0;
	for (i=0;i<input->nWalls;i++){
		sum+=((input->walls[i].x2 - input->walls[i].x1)*(input->walls[i].z2 + input->walls[i].z1));
	}
	return sum;
}

struct Sector *splitSector(struct Sector *input) {
  int i, j, k;
  int wallIdx;
	struct Sector *result = splitWork(input);
	//TODO find a better heuristic for determining if splits are malformed
	printf("Curve for split 0: %.0f\n",edgeSum(&result[0]));
	printf("Curve for split 1: %.0f\n",edgeSum(&result[1]));
  //if (edgeSum(&result[0])!=edgeSum(&result[1])) {
  //  printf("splits were done incorrectly: %d, needed %d. Reversing interiors\n",result[0].nWalls + result[1].nWalls, input->nWalls);
  //  for (i = 0; i < input->nWalls; i++) {
  //    if (input->walls[i].type == INTERIOR)
  //      reverseWall(&input->walls[i]);
  //  }
	//	free(result[0].walls);
	//	free(result[1].walls);
	//	free(result);
	//	result = splitWork(input);
  //}
  return result;
}

struct Sector slurpFromFile(char *input) {
  struct Sector result, resultForSplit;
  struct Sector *splits;
  char ch;
  enum SlurpState state = WALLCOUNT;
  enum SectorState sectorState = PERIMETERDEF;
  Vector2 startPoint;
  int startSaved = 0;
  int lineN = 0;
  int wallIdx = 0;
  int nHoles = 0;
  int i, j;
  // wildly assuming there will be less than 256 holes in a given sector
  int holeIndexes[256];
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
      switch (sectorState) {
      case (PERIMETERDEF):
        result.walls[wallIdx].type = PERIMETER;
        break;
      case (HOLEDEF):
        result.walls[wallIdx].type = INTERIOR;
        break;
      }
      if (!startSaved) {
        startPoint =
            (Vector2){result.walls[wallIdx].x1, result.walls[wallIdx].z1};
        startSaved = 1;

        // when a new hole is started, I want to save the index of its first
        // point so that addBridgesToSector has to do less math
        if (sectorState == HOLEDEF)
          holeIndexes[nHoles] = wallIdx;
      }
      int atStart =
          Vector2Equals(startPoint, (Vector2){result.walls[wallIdx].x2,
                                              result.walls[wallIdx].z2});
      if (atStart) {
        switch (sectorState) {
        case (PERIMETERDEF):
          // TODO sort the points in the perimiter clockwise
          // printf("outer loop closed, ready to read holes\n");
          sectorState = HOLEDEF;
          startSaved = 0;
          break;
        case (HOLEDEF):
          // TODO save start/end index for every hole
          // TODO sort the points in the hole counter-clockwise
          // printf("hole %d closed, ready to read another\n", nHoles);
          nHoles++;
          startSaved = 0;
        }
      }

      wallIdx++;
    }
  }

  fclose(file);
  result.nHoles = nHoles;
  result.holeIndexes = malloc(sizeof(int) * nHoles);
  for (int i = 0; i < nHoles; i++) {
    result.holeIndexes[i] = holeIndexes[i];
  }

  // skip comments "#"
  // print out the number of walls
  // print out wall n goes from x1,z1 to x2,x2

  // TODO sort points on perimeter clockwise and points on holes
  // counterclockwise by following the paths and calculating if the angle
  // between all of them is positive or negative
  if (nHoles > 0) {
    // I copy result into resultForSplit so that the extra lines (walls) added
    // by it are not passed to the final result, as this would add nHoles+1
    // calls to the wall drawing function after the level loads, every frame
    // after the number of triangles is calculated, I only need those
    // triangles, not the extra walls
    // TODO uncomment the below line to prevent polluting the sector with
    // extra walls
    int oldNWalls = result.nWalls;
    addBridgesToSector(&result, nHoles);
    result.nfloorTris = 0;
    splits = splitSector(&result);
    splits[0].nfloorTris = 0;
    splits[1].nfloorTris = 0;
    triangulate(&splits[0]);
    triangulate(&splits[1]);
    result.nfloorTris = splits[0].nfloorTris + splits[1].nfloorTris;
    result.triangles = malloc(sizeof(struct Triangle) * result.nfloorTris);
    for (i = 0; i < splits[0].nfloorTris; i++) {
      result.triangles[i] = splits[0].triangles[i];
    }
    for (j = 0; j < splits[1].nfloorTris; j++) {
      result.triangles[j + i] = splits[1].triangles[j];
    }
    result.nWalls = oldNWalls;
    result.walls = realloc(result.walls, sizeof(struct Wall) * oldNWalls);
    free(splits[0].walls);
    free(splits[1].walls);
    if (splits[0].nfloorTris > 0)
      free(splits[0].triangles);
    if (splits[1].nfloorTris > 0)
      free(splits[1].triangles);
    free(splits);
  } else {
    triangulate(&result);
  }

  return result;
}

float Vector2CrossProduct(Vector2 a, Vector2 b) {
  return (a.x * b.y) - (a.y * b.x);
}

int triangleHasPointInside(int a, int b, int c, struct Triangle tempTriangle,
                           bool *checkList, struct Sector *input) {
  int i;
  Vector2 currentPoint;
  Vector2 aPoint, bPoint, cPoint, pPoint;
  Vector2 ab, bc, ca;
  Vector2 ap, bp, cp;
  float cross1, cross2, cross3;
  // get the three points of the triangle
  aPoint = (Vector2){input->walls[a].x1, input->walls[a].z1};
  bPoint = (Vector2){input->walls[b].x1, input->walls[b].z1};
  cPoint = (Vector2){input->walls[c].x1, input->walls[c].z1};
  ab = Vector2Subtract(bPoint, aPoint);
  bc = Vector2Subtract(cPoint, bPoint);
  ca = Vector2Subtract(aPoint, cPoint);

  for (i = 0; i < input->nWalls; i++) {
    if (checkList[i])
      continue;
    if (!((i == a) || (i == b) || (i == c))) {
      pPoint = (Vector2){input->walls[i].x1, input->walls[i].z1};
      // printf("checking %.0f, %.0f against %d %d %d.\n", pPoint.x, pPoint.y,
      //        a + 1, b + 1, c + 1);
      //  if the point we're checking occupies the exact same space as one of
      //  ab or c, ignore it I could've wrapped this into the above if
      //  statement but it would be too long
      if ((pPoint.x == aPoint.x && pPoint.y == aPoint.y) ||
          (pPoint.x == bPoint.x && pPoint.y == bPoint.y) ||
          (pPoint.x == cPoint.x && pPoint.y == cPoint.y)) {
        continue;
      }
      ap = Vector2Subtract(pPoint, aPoint);
      bp = Vector2Subtract(pPoint, bPoint);
      cp = Vector2Subtract(pPoint, cPoint);
      cross1 = Vector2CrossProduct(ab, ap);
      cross2 = Vector2CrossProduct(bc, bp);
      cross3 = Vector2CrossProduct(ca, cp);

      if (!(cross1 > 0.0f || cross2 > 0.0f || cross3 > 0.0f)) {
        return 1;
      }
    }
  }
  return 0;
}
// takes the average of all the points
// is this right?
Vector2 getSectorCenter(struct Sector *input) {
  int i;
  float x = 0;
  float z = 0;
  for (i = 0; i < input->nWalls; i++) {
    x += input->walls[i].x1;
    z += input->walls[i].z1;
  }
  x /= input->nWalls;
  z /= input->nWalls;
  return (Vector2){x, z};
}

// lord forgive me for using a global variable
static Vector2 center;
int comparePointClockwise(const void *a, const void *b) {
  Vector2 aPoint, bPoint;
  // if (((struct Wall *)a)->type == INTERIOR)
  //   aPoint = (Vector2){((struct Wall *)a)->x2 - center.x,
  //                      ((struct Wall *)a)->z2 - center.y};
  // else
  aPoint = (Vector2){((struct Wall *)a)->x1 - center.x,
                     ((struct Wall *)a)->z1 - center.y};

  // if (((struct Wall *)b)->type == INTERIOR)
  //   bPoint = (Vector2){((struct Wall *)b)->x2 - center.x,
  //                      ((struct Wall *)b)->z2 - center.y};
  // else
  bPoint = (Vector2){((struct Wall *)b)->x1 - center.x,
                     ((struct Wall *)b)->z1 - center.y};
  float angle = Vector2Angle(aPoint, bPoint);
  if (angle > 0)
    return 1;
  else if (angle < 0)
    return -1;

  return 0;
}

void sortSectorWallsClockwise(struct Sector *input) {
  // define the center point of the sector
  center = getSectorCenter(input);
  qsort(input->walls, input->nWalls, sizeof(struct Wall),
        comparePointClockwise);
  return;
}

void triangulate(struct Sector *input) {
  int i, j = 0;
  Vector2 currentPoint, previousPoint, nextPoint;
  Vector2 currentToPrev, currentToNext;
  struct Triangle tempTriangle;
  // sortSectorWallsClockwise(input);
  input->nfloorTris = input->nWalls - 2;
  bool *checkList = calloc(input->nWalls, sizeof(bool));
  input->triangles = malloc(sizeof(struct Triangle) * input->nfloorTris);
  int currentWall = 0;
  int remainingWalls = input->nWalls;
  int previousWall, nextWall;
  while (remainingWalls > 3) {
    if (currentWall >= input->nWalls)
      currentWall = 0;
    // skip completed walls
    if (checkList[currentWall]) {
      currentWall++;
      continue;
    }
    // determine nearest previous wall unused
    previousWall = currentWall - 1;
    while (true) {
      if (previousWall < 0)
        previousWall = input->nWalls - 1;
      if (!checkList[previousWall])
        break;
      previousWall--;
    }
    // determine nearest next wall unused
    nextWall = currentWall + 1;
    while (true) {
      if (nextWall >= input->nWalls)
        nextWall = 0;
      if (!checkList[nextWall])
        break;
      nextWall++;
    }
    currentPoint =
        (Vector2){input->walls[previousWall].x1, input->walls[previousWall].z1};
    previousPoint =
        (Vector2){input->walls[currentWall].x1, input->walls[currentWall].z1};
    nextPoint = (Vector2){input->walls[nextWall].x1, input->walls[nextWall].z1};

    currentToPrev = Vector2Subtract(currentPoint, previousPoint);
    currentToNext = Vector2Subtract(currentPoint, nextPoint);
    if (Vector2CrossProduct(currentToPrev, currentToNext) < 0.0f) {
      tempTriangle = (struct Triangle){currentPoint, previousPoint, nextPoint};
      if (!triangleHasPointInside(currentWall, nextWall, previousWall,
                                  tempTriangle, checkList, input)) {
        input->triangles[j++] = tempTriangle;
        checkList[currentWall] = 1;
        remainingWalls--;
        currentWall = 0;
        continue;
      }
    }
    currentWall++;
    if (currentWall == input->nWalls) {
      printf("Made a whole loop of all points without clipping an ear...\n");
      break;
    }
  }
  i = 0;
  while (checkList[i]) {
    i++;
  }
  previousWall = i;
  i++;
  while (checkList[i]) {
    i++;
  }
  currentWall = i;
  i++;
  while (checkList[i]) {
    i++;
  }
  nextWall = i;

  // TODO maybe sort *these* clockwise
  currentPoint =
      (Vector2){input->walls[currentWall].x1, input->walls[currentWall].z1};
  nextPoint =
      (Vector2){input->walls[previousWall].x1, input->walls[previousWall].z1};
  previousPoint =
      (Vector2){input->walls[nextWall].x1, input->walls[nextWall].z1};
  input->triangles[j++] =
      (struct Triangle){currentPoint, previousPoint, nextPoint};

  if (remainingWalls == 3) {
    free(checkList);
  } else
    fprintf(stderr, "Triangulation algorithm failed, didn't reduce edges to 3 "
                    "before exiting.\n This will lead to a memory leak.");
  return;
}
// because 0 y is the top of the screen, shapes willl render upside down without
// this
void flipShape(struct Sector *input) {
  int i;
  float highestPoint = input->walls[0].z1;
  Vector2 temp;
  for (i = 0; i < input->nWalls; i++) {
    if (input->walls[i].z1 > highestPoint)
      highestPoint = input->walls[i].z1;
  }
  for (i = 0; i < input->nWalls; i++) {
    input->walls[i].z1 *= -1;
    input->walls[i].z1 += highestPoint;
    input->walls[i].z2 *= -1;
    input->walls[i].z2 += highestPoint;
  }
  for (i = 0; i < input->nfloorTris; i++) {
    input->triangles[i].a.y *= -1;
    input->triangles[i].a.y += highestPoint;
    input->triangles[i].b.y *= -1;
    input->triangles[i].b.y += highestPoint;
    input->triangles[i].c.y *= -1;
    input->triangles[i].c.y += highestPoint;
    temp = input->triangles[i].a;
    input->triangles[i].a = input->triangles[i].b;
    input->triangles[i].b = temp;
  }
}

// shifts, and scales the shape to fit better on the screen
void offsetShape(struct Sector *input, Vector2 center, Vector2 shapeCenter) {
  int i;
  for (i = 0; i < input->nfloorTris; i++) {
    input->triangles[i].a = Vector2Subtract(input->triangles[i].a, shapeCenter);
    input->triangles[i].b = Vector2Subtract(input->triangles[i].b, shapeCenter);
    input->triangles[i].c = Vector2Subtract(input->triangles[i].c, shapeCenter);
    input->triangles[i].a = Vector2Scale(input->triangles[i].a, 80.0);
    input->triangles[i].b = Vector2Scale(input->triangles[i].b, 80.0);
    input->triangles[i].c = Vector2Scale(input->triangles[i].c, 80.0);
    input->triangles[i].a = Vector2Add(input->triangles[i].a, center);
    input->triangles[i].b = Vector2Add(input->triangles[i].b, center);
    input->triangles[i].c = Vector2Add(input->triangles[i].c, center);
  }
  for (i = 0; i < input->nWalls; i++) {
    input->walls[i].x1 -= shapeCenter.x;
    input->walls[i].x2 -= shapeCenter.x;
    input->walls[i].z1 -= shapeCenter.y;
    input->walls[i].z2 -= shapeCenter.y;

    input->walls[i].x1 *= 80.0;
    input->walls[i].x2 *= 80.0;
    input->walls[i].z1 *= 80.0;
    input->walls[i].z2 *= 80.0;

    input->walls[i].x1 += center.x;
    input->walls[i].x2 += center.x;
    input->walls[i].z1 += center.y;
    input->walls[i].z2 += center.y;
  }
}

// you can give this a txt file with a list of walls, it will sort them and
// display the polygon they form divided into triangles
int main(int argc, char **argv) {

  /* log levels
   * LOG_ALL: 0
   * LOG_TRACE: 1
   * LOG_DEBUG: 2
   * LOG_INFO: 3
   * LOG_WARNING: 4
   * LOG_ERROR: 5
   * LOG_FATAL: 6
   * LOG_NONE: 7
   */
  SetTraceLogLevel(4);

  InitWindow(screenWidth, screenHeight,
             "raylib [polygon triangulation] example - ear clipping");
  SetTargetFPS(60);
  struct Sector sector1 = slurpFromFile(argv[1]);
  int screenX = GetRenderWidth();
  int screenY = GetRenderHeight();
  flipShape(&sector1);
  Vector2 screenCenter = (Vector2){screenX / 2.0, screenY / 2.0};
  Vector2 shapeCenter = getSectorCenter(&sector1);
  offsetShape(&sector1, screenCenter, shapeCenter);

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    BeginDrawing();

    ClearBackground(GRUVBOX_BACKGROUND);

    DrawText("triangulation example", 20, 20, 20, GRUVBOX_TEXT);

    if (IsWindowResized()) {
      screenX = GetRenderWidth();
      screenY = GetRenderHeight();
      screenCenter = (Vector2){screenX / 2.0, screenY / 2.0};
    }
    drawSectorTris(sector1, screenCenter, shapeCenter);
    drawSectorOutline(sector1, screenCenter, shapeCenter);

    DrawLine(18, 42, screenWidth - 18, 42, BLACK);
    EndDrawing();
  }
  CloseWindow(); // Close window and OpenGL context
  printf("%d walls freeing on closure\n", sector1.nWalls);
  free(sector1.walls);
  printf("%d tris freeing on closure\n", sector1.nfloorTris);
  free(sector1.triangles);

  return 0;
}
