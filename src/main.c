#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tice.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graphx.h>
#include <keypadc.h>

// Include the snake logos
#include "gfx/all_gfx.h"

#define TRANSPARENT_COLOR   10  // Some random transparent color
#define WHITE_COLOR         255 // White
#define BLACK_COLOR         0   // Black
#define GREEN_COLOR         6   // Green
#define RED_COLOR           224 // Red

#define SQUARE_SIZE         10
#define TOP_SAFESPACE       2*SQUARE_SIZE
#define WIDTH_TOTAL_CORDS   ((LCD_WIDTH/SQUARE_SIZE)-1)
#define HEIGHT_TOTAL_CORDS  (((LCD_HEIGHT/SQUARE_SIZE)-1)-(TOP_SAFESPACE/SQUARE_SIZE))
#define TOTAL_CORDS         (WIDTH_TOTAL_CORDS*HEIGHT_TOTAL_CORDS) // Total possible coordinates (used for setting arrays)

#define ONE_SECOND          32768/1 // One second on the timer
#define HALF_SECOND         32768/2 // Half a second on the timer
#define QUARTER_SECOND      32768/4 // A quarter second on the timer


// Function declarations
void homeScreen();
void startGame();
void dieScreen(int score);

void main(void) {
    srand(rtc_Time()); // Seed random number generator with time
    homeScreen(); // Load the home screen
}

void homeScreen() {
    // Variables
    gfx_sprite_t *snake;

    unsigned seconds = 0;
    char str[10];
    bool startGameBlink = false;

    const char *title = "Snake";
    const char *startText = "[ENTER] Start game";
    const char *madeBy = "Made by Banakin";

    // Allocate space for the decompressed sprite
    snake = gfx_MallocSprite(snake_width, snake_height); // Same as: gfx_AllocSprite(apple_width, apple_height, malloc)

    // Decompress the sprite
    zx7_Decompress(snake, snake_compressed);
    
    // Initialize the 8bpp graphics
    gfx_Begin();

    // Clear the homescreen and set up palette
    os_ClrHome();
    gfx_SetPalette(all_gfx_pal, sizeof_all_gfx_pal, 0);

    // Fill the screen black
    gfx_FillScreen(BLACK_COLOR);

    // Set text BG and Transparent Color (Allows for white text)
    gfx_SetTextBGColor(TRANSPARENT_COLOR); // Set Text BG
    gfx_SetTextTransparentColor(TRANSPARENT_COLOR); // Set Text Transparent Color

    // Printing title and icon
    gfx_SetTextScale(5, 5); // Text size
    gfx_SetTextFGColor(6); // Text color
    gfx_PrintStringXY(title, (LCD_WIDTH - ((snake_width*13)+ 10 + gfx_GetStringWidth(title)))/2+((snake_width*13)+ 10), 10); // Print text
    gfx_ScaledSprite_NoClip(snake, (LCD_WIDTH - ((snake_width*13)+ 10 + gfx_GetStringWidth(title)))/2, 10, 13, 13); // Print icon (Sprite)

    // Printing "[enter] Start game"
    gfx_SetTextScale(2, 2); // Text size
    gfx_SetTextFGColor(WHITE_COLOR); // Text color
    gfx_PrintStringXY(startText, (LCD_WIDTH - gfx_GetStringWidth(startText))/2, LCD_HEIGHT/2); // Print text

    // Printing "Made by Banakin"
    gfx_SetTextScale(1, 1); // Text size
    gfx_SetTextFGColor(WHITE_COLOR); // Text color
    gfx_PrintStringXY(madeBy, (LCD_WIDTH - gfx_GetStringWidth(madeBy))/2, LCD_HEIGHT - 18); // Print text

    // Blinking text timer variables
    timer_Control = TIMER1_DISABLE; // Disable the timer so it doesn't run when we don't want it to be running
    timer_1_ReloadValue = timer_1_Counter = HALF_SECOND; // By using the 32768 kHz clock, we can count for exactly 1 second here, or a different interval of time
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN; // Enable the timer, set it to the 32768 kHz clock, enable an interrupt once it reaches 0, and make it count down
    
    // Loop until Clear is pressed
    while(true) {
        // Blinking text timer
        if (timer_IntStatus & TIMER1_RELOADED) {
            if (startGameBlink == false) {
                // Print a string
                gfx_SetTextScale(2, 2); // Text size
                gfx_SetTextFGColor(WHITE_COLOR); // Text color
                gfx_PrintStringXY(startText, (LCD_WIDTH - gfx_GetStringWidth(startText))/2, LCD_HEIGHT/2); // Print text
                startGameBlink = true;
            } else {
                gfx_FillRectangle((LCD_WIDTH - gfx_GetStringWidth(startText))/2, LCD_HEIGHT/2, gfx_GetStringWidth(startText), 16);
                startGameBlink = false;
            }
            // Acknowledge the reload
            timer_IntAcknowledge = TIMER1_RELOADED;
        }

        // Update kb_Data
        kb_Scan();

        if (kb_Data[6] == kb_Enter){
            // Stop the timer
            timer_Control = TIMER1_DISABLE;
            // Clear the sprite from memory
            free(snake);
            // Start the game
            startGame();
            break;
        }

        if (kb_Data[6] == kb_Clear){
            // Stop the timer
            timer_Control = TIMER1_DISABLE;
            // Clear the sprite from memory
            free(snake);
            // Close the graphics
            gfx_End();
            break;
        }
    }
}

void startGame() {
    // Variables
    kb_key_t arrowKeys;
    const char *pausedMessage = "Paused";
    bool enterPrevkey, isPaused;
    int snakeCords[TOTAL_CORDS];
    int snakeLength = 1;
    int goalCords[2] = {100, 80}; // Starting goal
    int gameScore = 0;
    int snakeDirection = 0; // 0 = right, 1 = left, 2 = up, 3 = down

    // Set start cords
    snakeCords[0] = 808;

    // Set pause bools
    enterPrevkey = false;
    isPaused = true;

    // Clear screen (Set it to black)
    gfx_FillScreen(BLACK_COLOR);

    // Snake movement timer variables
    timer_Control = TIMER1_DISABLE; // Disable the timer so it doesn't run when we don't want it to be running
    timer_1_ReloadValue = timer_1_Counter = QUARTER_SECOND; // By using the 32768 kHz clock, we can count for exactly 1 second here, or a different interval of time
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN; // Enable the timer, set it to the 32768 kHz clock, enable an interrupt once it reaches 0, and make it count down

    // Loop until Clear is pressed
    while (true) {
        // Snake movement timer
        if (timer_IntStatus & TIMER1_RELOADED) {
            int headCordsX = (int)(snakeCords[snakeLength-1] / 100)*SQUARE_SIZE;
            int headCordsY = (int)(snakeCords[snakeLength-1] % 100)*SQUARE_SIZE;
            int tailCordsX = (int)(snakeCords[0] / 100)*SQUARE_SIZE;
            int tailCordsY = (int)(snakeCords[0] % 100)*SQUARE_SIZE;
            char scoreCounter[15];
            sprintf(scoreCounter, "Score: %i", gameScore);

            // Show variables
            gfx_SetColor(BLACK_COLOR);
            gfx_FillRectangle(0, 0, LCD_WIDTH, TOP_SAFESPACE-1);
            gfx_SetTextScale(1, 1); // Text size
            gfx_SetTextFGColor(WHITE_COLOR); // Text color
            gfx_PrintStringXY(scoreCounter, 0, 0); // Print text

            // Set goal
            gfx_SetColor(RED_COLOR);
            gfx_FillRectangle(goalCords[0], goalCords[1], SQUARE_SIZE, SQUARE_SIZE);

            // Clear tail
            gfx_SetColor(BLACK_COLOR);
            gfx_FillRectangle(tailCordsX, tailCordsY, SQUARE_SIZE, SQUARE_SIZE);
            
            // Check head boundaries
            if (
                ((headCordsX+SQUARE_SIZE) >= LCD_WIDTH)     ||  // Right out of bounds (Cords cannot be bigger)
                ((headCordsY+SQUARE_SIZE) >= LCD_HEIGHT)    ||  // Bottom out of bounds (Cords cannot be bigger)
                ((headCordsX-SQUARE_SIZE) < 0)              ||  // Left out of bounds (Cords can be equal to)
                ((headCordsY-SQUARE_SIZE) < TOP_SAFESPACE)      // Top out of bounds (Cords can be equal to)
            ){
                // Disable timer
                timer_Control = TIMER1_DISABLE;
                // Kill the player
                dieScreen(gameScore);
                // Break loop
                break;
            }
            
            // If the head touches the goal
            if ((goalCords[0] == headCordsX) && (headCordsY == goalCords[1])) {
                // Incrament game score
                gameScore = gameScore + 1;

                // Get new location
                goalCords[0] = (rand() % WIDTH_TOTAL_CORDS)*SQUARE_SIZE; // Random number in safe space that is multiple of square size
                goalCords[1] = (rand() % HEIGHT_TOTAL_CORDS)*SQUARE_SIZE+TOP_SAFESPACE; // Random number in safe space that is multiple of square size
            }


            if (snakeDirection == 0) {
                // Move right
                snakeCords[snakeLength-1] = ((headCordsX/SQUARE_SIZE)+1)*100+((headCordsY/SQUARE_SIZE));
            } else if (snakeDirection == 1) {
                // Move left
                snakeCords[snakeLength-1] = ((headCordsX/SQUARE_SIZE)-1)*100+((headCordsY/SQUARE_SIZE));
            } else if (snakeDirection == 2) {
                // Move up
                snakeCords[snakeLength-1] = ((headCordsX/SQUARE_SIZE))*100+((headCordsY/SQUARE_SIZE)-1);
            } else if (snakeDirection == 3) {
                // Move down
                snakeCords[snakeLength-1] = ((headCordsX/SQUARE_SIZE))*100+((headCordsY/SQUARE_SIZE)+1);
            }

            // Set head
            gfx_SetColor(GREEN_COLOR);
            gfx_FillRectangle(headCordsX, headCordsY, SQUARE_SIZE, SQUARE_SIZE);

            // Set top border
            gfx_SetColor(WHITE_COLOR);
            gfx_FillRectangle(0, TOP_SAFESPACE, LCD_WIDTH, 1);

            // Acknowledge the reload
            timer_IntAcknowledge = TIMER1_RELOADED;
        }

        // Update kb_Data
        kb_Scan();

        // Arrow keys
        arrowKeys = kb_Data[7];
        if (!isPaused){
            switch (arrowKeys) {
                case kb_Right:
                    // If not coming from left, set to right
                    if (snakeDirection != 1) {
                        snakeDirection = 0;
                    }
                    break;

                case kb_Left:
                    // If not coming from right, set to left
                    if (snakeDirection != 0) {
                        snakeDirection = 1;
                    }
                    break;

                case kb_Up:
                    // If not coming from down, set to up
                    if (snakeDirection != 3) {
                        snakeDirection = 2;
                    }
                    break;

                case kb_Down:
                    // If not coming from down, set to up
                    if (snakeDirection != 2) {
                        snakeDirection = 3;
                    }
                    break;

                default:
                    break;
            }
        }

        // Check for enter key
        if (kb_Data[6] == kb_Enter && !enterPrevkey){
            if (!isPaused){
                // Disable the timer
                timer_Control = TIMER1_DISABLE;
                // Set the game to paused
                isPaused = true;
                // Clear the screen
                gfx_FillScreen(BLACK_COLOR);
                // Set show paused screen
                gfx_SetTextScale(2, 2); // Text size
                gfx_SetTextFGColor(WHITE_COLOR); // Text color
                gfx_PrintStringXY(pausedMessage, (LCD_WIDTH - gfx_GetStringWidth(pausedMessage))/2, LCD_HEIGHT/2); // Print text
            } else {
                // Clear the screen
                gfx_FillScreen(BLACK_COLOR);
                // Re-enable timer
                timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN;
                // Set the game to paused
                isPaused = false;
            }
        }
        enterPrevkey = kb_Data[6] == kb_Enter;

        // Check for clear
        if (kb_Data[6] == kb_Clear){
            // Close the graphics
            gfx_End();
            break;
        }
    }
}

void dieScreen(int gameScore) {
    #define DEATH_MESSAGE "You Died"

    // Variables
    char scoreMSG[18];
    sprintf(scoreMSG, "Finial Score: %i", gameScore);
    
    // Set text color and size
    gfx_SetTextScale(2, 2); // Text size
    gfx_SetTextFGColor(WHITE_COLOR); // Text color

    // Clear the screen
    gfx_FillScreen(BLACK_COLOR);

    // Set show death message
    gfx_PrintStringXY(DEATH_MESSAGE, (LCD_WIDTH - gfx_GetStringWidth(DEATH_MESSAGE))/2, LCD_HEIGHT/3); // Print text
    
    // Show score
    gfx_PrintStringXY(scoreMSG, (LCD_WIDTH - gfx_GetStringWidth(scoreMSG))/2, (LCD_HEIGHT/3)*2); // Print text

    while (true) {
        // Update kb_Data
        kb_Scan();

        // Exit on clear or enter
        if (kb_Data[6] == kb_Clear || kb_Data[6] == kb_Enter){
            // Close the graphics
            gfx_End();
            break;
        }
    }
}