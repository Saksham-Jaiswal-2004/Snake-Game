// System includes and dependencies
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

    // Function: kbhit()
    // Purpose: Non-blocking check for keyboard input on Unix systems
    // Returns: 1 if a key is pressed, 0 otherwise
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

    // Function: getch()
    // Purpose: Get a single character from keyboard without echo
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

    // Function: wait()
    // Purpose: Wait for user input while maintaining terminal settings
    void wait(void){
        struct termios oldt, newt;
        fflush(stdout);  // Make sure the message is printed immediately

        // Get current terminal settings
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;

        // Disable canonical mode and echo
        newt.c_lflag &= ~(ICANON | ECHO);

        // Apply new settings
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        // Wait for any key
        getchar();

        // Restore original terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

        printf("\n");
    }
#endif

// Game constants
#define WIDTH 50          // Game board width
#define HEIGHT 20         // Game board height
#define GAME_DATA_FILE "game_data.txt" // File path for game session data
#define HIGH_SCORE_FILE "highscore.txt" // File path for high score data
#define FOOD_SOUND "assets/food.wav" // Sound file for food consumption
#define GAME_SOUND "assets/select.wav" // Sound file for game start
#define GAME_OVER_SOUND "assets/gameover.wav" // Sound file for game over
#define POISON_SOUND "assets/poison.wav" // Sound file for poison consumption
#define GOLDENFOOD_SOUND "assets/bonus.wav" // Sound file for golden food consumption
#define PAUSE_SOUND "assets/Pause.wav" // Sound file for game pause

// ANSI color codes for terminal output
#define GREEN "\033[1;32m"
#define RED "\033[1;31m"
#define YELLOW "\033[1;33m"
#define CYAN "\033[1;36m"
#define BLUE "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define RESET "\033[0m"

#define MAX_OBSTACLES 10  // Maximum number of obstacles in the game

// Snake body segment structure
typedef struct Node {
    int x, y;            // Position coordinates
    struct Node* next;   // Pointer to next body segment
} Node;

// Global game state variables
Node *head = NULL, *tail = NULL;    // Snake head and tail pointers
int food = 0, foodX, foodY;         // Food status and position
int poisonX = -1, poisonY = -1;     // Poison position (-1 means not present)
int goldenFoodX = -1, goldenFoodY = -1;  // Golden food position
int score = 0, highScore = 0;       // Score tracking
int gameOver = 0, paused = 0;       // Game state flags
int foodCounter = 0;                // Counter for food generation
char direction = 'R';               // Current snake direction (R/L/U/D)
char player[100];                   // Player name storage
int level = 1;                      // Current game level

// Obstacle tracking
int obstacleCount = 0;              // Number of active obstacles
int obstacles[MAX_OBSTACLES][2];     // Obstacle positions array

// Utility Functions
void clearScreen() {
    // Platform-specific screen clearing
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Console cursor control functions
void hideCursor() {
    #ifdef _WIN32
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(out, &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(out, &cursorInfo);
    #else
        printf("\e[?25l");
        fflush(stdout);
    #endif
}

void showCursor() {
    #ifdef _WIN32
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(out, &cursorInfo);
        cursorInfo.bVisible = TRUE;
        SetConsoleCursorInfo(out, &cursorInfo);
    #else
        printf("\e[?25h");
        fflush(stdout);
    #endif
}

// Game UI Functions
void loader(const char* msg) {
    // Displays loading animation with message
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
    // Displays main game menu
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

// Sound Functions
void playSound(const char *filename) {
    // Cross-platform sound playing implementation
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

// Game Logic Functions
void generateFood() {
    // Generates food at random positions
    // Also handles poison and golden food generation
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
    // Creates obstacles based on current level
    obstacleCount = (level == 2) ? 3 : (level == 3) ? 6 : 0;
    for (int i = 0; i < obstacleCount; i++) {
        obstacles[i][0] = rand() % (WIDTH - 2) + 1;
        obstacles[i][1] = rand() % (HEIGHT - 2) + 1;
    }
}

void initSnake() {
    // Initializes snake at starting position
    head = malloc(sizeof(Node));
    head->x = WIDTH / 2;
    head->y = HEIGHT / 2;
    head->next = NULL;
    tail = head;
}

// File Handling Functions
void loadHighScore() {
    // Loads high score from file
    FILE *file = fopen(HIGH_SCORE_FILE, "r");
    if (file) {
        fscanf(file, "%d", &highScore);
        fclose(file);
    }
}

void saveHighScore() {
    // Saves new high score if achieved
    if (score > highScore) {
        FILE *file = fopen(HIGH_SCORE_FILE, "w");
        fprintf(file, "%d", score);
        fclose(file);
    }
}

void saveGameData() {
    // Records game session data with timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timeStr[100];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);

    FILE *file = fopen(GAME_DATA_FILE, "a");
    fprintf(file, "Player: %s\nScore: %d\nDate and Time: %s\n\n", player, score, timeStr);
    fclose(file);
}

// Game State Display Functions
void showHighScore() {
    // Displays high score
    clearScreen();
    loadHighScore();
    printf(GREEN "\nHigh Score: %d\n" RESET, highScore);
    printf("\nPress any key to go back to menu...");
    #ifdef _WIN32
    getch();
    #else
    wait();
    getch();
    #endif
}

void showPastScores() {
    // Displays past scores from file
    clearScreen();
    FILE *file = fopen(GAME_DATA_FILE, "r");
    if (file) {
        char ch;
        while ((ch = fgetc(file)) != EOF) putchar(ch);
        fclose(file);
    } else {
        printf(RED "\nNo past scores found.\n" RESET);
    }
    printf("\nPress any key to go back to menu...");
    #ifdef _WIN32
    getch();
    #else
    wait();
    getch();
    #endif
}

void draw() {
    // Renders game board and all elements
    // Including snake, food, obstacles, and UI
    clearScreen();
    for (int i = 0; i < WIDTH + 2; i++) printf(BLUE "*" RESET);
    printf("\n");

    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH + 2; j++) {
            if (j == 0 || j == WIDTH + 1)
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

    for (int i = 0; i < WIDTH + 2; i++) printf(BLUE "*" RESET);
    printf("\n" YELLOW "Score: %d | High Score: %d\n" RESET, score, highScore);
    if(paused)
    printf("\n" CYAN "Press 'p' to Unpause Game" RESET);
    else
    printf("\n" CYAN "Press 'p' to Pause Game" RESET);
    printf("\n" CYAN "Press 'x' to Exit Game" RESET);
}

void input() {
    // Handles keyboard input for game control
    // Supports both arrow keys and WASD
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
    // Updates snake position based on direction
    // Handles collisions and food consumption
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
        // REMOVE tail twice (shrink length by 2 if possible)
        for (int i = 0; i < 2 && head != tail; i++) {
            Node* temp = head;
            while (temp->next->next) temp = temp->next;
            free(temp->next);
            temp->next = NULL;
            tail = temp;
        }
        // Optional: if snake too short, end game
        if (head == tail) {
            gameOver = 1;
        }
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
    // Cleanup function to free snake memory
    while (head) {
        Node* temp = head;
        head = head->next;
        free(temp);
    }
}

void gameLoop() {
    // Main game loop handler
    // Controls game flow and updates
    printf("Enter Player's Name: ");
    scanf(" %[^\n]", player);
    loader("Loading Game...");
    playSound(GAME_SOUND);
    clearScreen();
    hideCursor();

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
    showCursor();
    printf("Press any key to return to menu...");
    getch();
}

int main() {
    // Program entry point
    // Initializes game and handles menu selection
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