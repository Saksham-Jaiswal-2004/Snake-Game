#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #define Sleep(x) usleep((x) * 1000)

    int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);             // save old settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);           // disable buffering
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);    // apply new settings
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();                             // read input

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);    // restore settings
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);                      // put character back
        return 1;
    }

    return 0;
}

int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);           // disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();                             // read char
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);    // restore terminal
    return ch;
}
#endif

#define WIDTH 50
#define HEIGHT 30
#define GAME_DATA_FILE "game_data.txt"
#define HIGH_SCORE_FILE "highscore.txt"
#define FOOD_SOUND "assets/food.wav"
#define GAME_SOUND "assets/select.wav"
#define GAME_OVER_SOUND "assets/gameover.wav"
#define POISON_SOUND "assets/poison.wav"
#define GOLDENFOOD_SOUND "assets/bonus.wav"
#define PAUSE_SOUND "assets/Pause.wav"

#define GREEN "\033[1;32m"
#define RED "\033[1;31m"
#define YELLOW "\033[1;33m"
#define CYAN "\033[1;36m"
#define BLUE "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define RESET "\033[0m"

#define MAX_OBSTACLES 10

typedef struct Node {
    int x, y;
    struct Node* next;
} Node;

Node *head = NULL, *tail = NULL;
int food = 0, foodX, foodY, poisonX = -1, poisonY = -1, goldenFoodX = -1, goldenFoodY = -1;
int score = 0, highScore = 0, gameOver = 0, paused = 0, foodCounter = 0;
char direction = 'R';
char player[100];
int level = 1;

int obstacleCount = 0;
int obstacles[MAX_OBSTACLES][2];

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void centerPrint(const char* msg) {
    int padding = (WIDTH - (int)strlen(msg)) / 2 + 1;
    for (int i = 0; i < padding; i++) printf(" ");
    printf("%s\n", msg);
}

void loader(const char* msg) {
    clearScreen();
    printf(MAGENTA "%s\n",msg);
    for (int i = 0; i < 3; i++) {
        printf(".");
        fflush(stdout);
        Sleep(400);
    }
    printf("\n" RESET);
    Sleep(300);
    clearScreen();
}

void showMenu() {
    clearScreen();
    printf(GREEN "Snake Game Menu\n" RESET);
    printf(YELLOW "1. Beginner\n");
    printf("2. Intermediate\n");
    printf("3. Advanced\n");
    printf("4. Past Scores\n");
    printf("5. High Score\n");
    printf("6. Exit\n" RESET);
    printf(MAGENTA "\nChoose an option (1-6): " RESET);
}

void playSound(const char *filename) {
    char command[200];
#ifdef _WIN32
    sprintf(command, "powershell -c \"(New-Object Media.SoundPlayer '%s').PlaySync();\"", filename);
#elif __APPLE__
    sprintf(command, "afplay \"%s\" &", filename);
#else
    sprintf(command, "aplay \"%s\" >/dev/null 2>&1 &", filename);
#endif
    system(command);
}

void generateFood() {
    foodX = rand() % (WIDTH - 2) + 1;
    foodY = rand() % (HEIGHT - 2) + 1;
    foodCounter++;
    if (foodCounter % 2 == 0) {
        poisonX = rand() % (WIDTH - 2) + 1;
        poisonY = rand() % (HEIGHT - 2) + 1;
        goldenFoodX = rand() % (WIDTH - 2) + 1;
        goldenFoodY = rand() % (HEIGHT - 2) + 1;
    }
}

void generateObstacles() {
    obstacleCount = (level == 2) ? 3 : (level == 3) ? 6 : 0;
    for (int i = 0; i < obstacleCount; i++) {
        obstacles[i][0] = rand() % (WIDTH - 2) + 1;
        obstacles[i][1] = rand() % (HEIGHT - 2) + 1;
    }
}

void initSnake() {
    head = malloc(sizeof(Node));
    head->x = WIDTH / 2;
    head->y = HEIGHT / 2;
    head->next = NULL;
    tail = head;
}

void loadHighScore() {
    FILE *file = fopen(HIGH_SCORE_FILE, "r");
    if (file) {
        fscanf(file, "%d", &highScore);
        fclose(file);
    }
}

void saveHighScore() {
    if (score > highScore) {
        FILE *file = fopen(HIGH_SCORE_FILE, "w");
        fprintf(file, "%d", score);
        fclose(file);
    }
}

void saveGameData() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timeStr[100];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);

    FILE *file = fopen(GAME_DATA_FILE, "a");
    fprintf(file, "Player: %s\nScore: %d\nDate and Time: %s\n\n", player, score, timeStr);
    fclose(file);
}

void showHighScore() {
    loadHighScore();
    printf(GREEN "\nHigh Score: %d\n" RESET, highScore);
    printf("\nPress any key to go back to menu...");
    getch();
}

void showPastScores() {
    FILE *file = fopen(GAME_DATA_FILE, "r");
    if (file) {
        char ch;
        while ((ch = fgetc(file)) != EOF) putchar(ch);
        fclose(file);
    } else {
        printf(RED "\nNo past scores found.\n" RESET);
    }
    printf("\nPress any key to go back to menu...");
    getch();
}

void draw() {
    clearScreen();
    // for (int i = 0; i < WIDTH + 2; i++) printf(BLUE "█" RESET);
    for (int i = 0; i < WIDTH + 2; i++) printf(BLUE "*" RESET);
    printf("\n");

    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH + 2; j++) {
            if (j == 0 || j == WIDTH + 1)
                // printf(BLUE "█" RESET);
                printf(BLUE "*" RESET);
            else if (paused && i == HEIGHT / 2 && j == (WIDTH - 6) / 2)
                { printf(RED "Paused" RESET); j += 5; }
            else if (gameOver && i == HEIGHT / 2 && j == (WIDTH - 9) / 2)
                { printf(RED "Game Over" RESET); j += 8; }
            else if (i == poisonY && j == poisonX)
                printf(RED "P" RESET);
            else if (i == goldenFoodY && j == goldenFoodX)
                printf(GREEN "$" RESET);
            else if (i == foodY && j == foodX)
                printf(YELLOW "@" RESET);
            else {
                int isBody = 0;
                for (Node* temp = head; temp; temp = temp->next) {
                    if (temp->x == j && temp->y == i) {
                        if (temp == head)
                            printf(CYAN "O" RESET);
                        else
                            printf(GREEN "o" RESET);
                        isBody = 1;
                        break;
                    }
                }
                int isObs = 0;
                for (int o = 0; o < obstacleCount; o++) {
                    if (obstacles[o][0] == j && obstacles[o][1] == i) {
                        printf(MAGENTA "#" RESET);
                        isObs = 1;
                        break;
                    }
                }
                if (!isBody && !isObs) printf(" ");
            }
        }
        printf("\n");
    }

    // for (int i = 0; i < WIDTH + 2; i++) printf(BLUE "█" RESET);
    for (int i = 0; i < WIDTH + 2; i++) printf(BLUE "*" RESET);
    printf("\n" YELLOW "Score: %d | High Score: %d\n" RESET, score, highScore);
    if(paused)
    printf("\n" CYAN "Press 'p' to Unpause Game" RESET);
    else
    printf("\n" CYAN "Press 'p' to Pause Game" RESET);
    printf("\n" CYAN "Press 'x' to Exit Game" RESET);
}

void input() {
    if (kbhit()) {
        char key = getch();
        if (key == 27) { // ESC
            if (kbhit() && getch() == '[') {
                char arrow = getch();
                if (arrow == 'A' && direction != 'D') direction = 'U';  // Up
                else if (arrow == 'B' && direction != 'U') direction = 'D';  // Down
                else if (arrow == 'C' && direction != 'L') direction = 'R';  // Right
                else if (arrow == 'D' && direction != 'R') direction = 'L';  // Left
            }
        } else {
            // WASD fallback
            if ((key == 'w' || key == 'W') && direction != 'D') direction = 'U';
            else if ((key == 's' || key == 'S') && direction != 'U') direction = 'D';
            else if ((key == 'a' || key == 'A') && direction != 'R') direction = 'L';
            else if ((key == 'd' || key == 'D') && direction != 'L') direction = 'R';
            else if (key == 'x' || key == 'X') gameOver = 1;
            else if (key == 'p' || key == 'P') {paused = !paused; playSound(PAUSE_SOUND);}
        }
    }
}

void moveSnake() {
    if (paused) return;

    int newX = head->x, newY = head->y;
    if (direction == 'U') newY--;
    else if (direction == 'D') newY++;
    else if (direction == 'L') newX--;
    else if (direction == 'R') newX++;

    if (newX == 0 || newX == WIDTH + 1 || newY < 0 || newY >= HEIGHT) {
        gameOver = 1;
        return;
    }

    for (Node* temp = head; temp; temp = temp->next) {
        if (temp->x == newX && temp->y == newY) {
            gameOver = 1;
            return;
        }
    }

    for (int i = 0; i < obstacleCount; i++) {
        if (obstacles[i][0] == newX && obstacles[i][1] == newY) {
            gameOver = 1;
            return;
        }
    }

    Node* newHead = malloc(sizeof(Node));
    newHead->x = newX;
    newHead->y = newY;
    newHead->next = head;
    head = newHead;

    if (newX == foodX && newY == foodY) {
        score += 10;
        generateFood();
        playSound(FOOD_SOUND);
    } else if (newX == poisonX && newY == poisonY) {
        score -= 10;
        poisonX = poisonY = -1;
        playSound(POISON_SOUND);
    } else if (newX == goldenFoodX && newY == goldenFoodY) {
        score += 30;
        goldenFoodX = goldenFoodY = -1;
        playSound(GOLDENFOOD_SOUND);
    } else {
        Node* temp = head;
        while (temp->next->next) temp = temp->next;
        free(temp->next);
        temp->next = NULL;
        tail = temp;
    }
}

void freeSnake() {
    while (head) {
        Node* temp = head;
        head = head->next;
        free(temp);
    }
}

void gameLoop() {
    printf("Enter Player's Name: ");
    scanf(" %[^\n]", player);
    loader("Loading Game...");
    playSound(GAME_SOUND);
    clearScreen();

    generateFood();
    generateObstacles();
    initSnake();
    loadHighScore();

    while (!gameOver) {
        draw();
        input();
        moveSnake();
        Sleep(100);
    }

    saveGameData();
    saveHighScore();
    playSound(GAME_OVER_SOUND);
    printf(RED "\nGame Over! Your final score: %d\n" RESET, score);
    freeSnake();
    printf("Press any key to return to menu...");
    getch();
}

int main() {
    srand(time(NULL));

    while (1) {
        showMenu();
        int choice;
        scanf("%d", &choice);

        switch (choice) {
            case 1: level = 1; gameOver = score = 0; loader("Starting Beginner Mode"); gameLoop(); break;
            case 2: level = 2; gameOver = score = 0; loader("Starting Intermediate Mode"); gameLoop(); break;
            case 3: level = 3; gameOver = score = 0; loader("Starting Advanced Mode"); gameLoop(); break;
            case 4: loader("Loading Past Scores"); showPastScores(); break;
            case 5: loader("Loading High Score"); showHighScore(); break;
            case 6: exit(0);
            default: printf("Invalid choice. Try again.\n"); Sleep(1000);
        }
    }
    return 0;
}