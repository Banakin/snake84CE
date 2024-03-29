#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>
#include <compression.h>

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
#define SIXTH_SECOND        32768/6 // A sixth of a second on the timer

#define TITLE "Snake"
#define START_TEXT "[ENTER] Start game"
#define VERSION_NUM "v1.1.0"
#define MADE_BY "Made by Brendan"

#define DEATH_MESSAGE "You Died"
#define PLAY_AGAIN_MSG "[ENTER] Play again"
#define EXIT_MSG "[CLEAR] Exit game"


// Function declarations
void homeScreen();
void startGame();
void dieScreen(uint16_t score);
uint16_t handleHighScore(uint16_t score);

int main(void) {
    srand(rtc_Time()); // Seed random number generator with time
    homeScreen(); // Load the home screen
    return 0;
}

void homeScreen() {
    // Variables
    gfx_sprite_t *snake;

    bool startGameBlink = false;

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
    gfx_PrintStringXY(TITLE, (LCD_WIDTH - ((snake_width*13)+ 10 + gfx_GetStringWidth(TITLE)))/2+((snake_width*13)+ 10), 10); // Print text
    gfx_ScaledSprite_NoClip(snake, (LCD_WIDTH - ((snake_width*13)+ 10 + gfx_GetStringWidth(TITLE)))/2, 10, 13, 13); // Print icon (Sprite)

    // Printing "[ENTER] Start game"
    gfx_SetTextScale(2, 2); // Text size
    gfx_SetTextFGColor(WHITE_COLOR); // Text color
    gfx_PrintStringXY(START_TEXT, (LCD_WIDTH - gfx_GetStringWidth(START_TEXT))/2, LCD_HEIGHT/2); // Print text

    // Printing version number
    gfx_SetTextScale(1, 1); // Text size
    gfx_SetTextFGColor(WHITE_COLOR); // Text color
    gfx_PrintStringXY(VERSION_NUM, (LCD_WIDTH - gfx_GetStringWidth(VERSION_NUM))/2, LCD_HEIGHT - 18*2); // Print text

    // Printing "Made by Brendan"
    gfx_SetTextScale(1, 1); // Text size
    gfx_SetTextFGColor(WHITE_COLOR); // Text color
    gfx_PrintStringXY(MADE_BY, (LCD_WIDTH - gfx_GetStringWidth(MADE_BY))/2, LCD_HEIGHT - 18); // Print text

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
                gfx_PrintStringXY(START_TEXT, (LCD_WIDTH - gfx_GetStringWidth(START_TEXT))/2, LCD_HEIGHT/2); // Print text
                startGameBlink = true;
            } else {
                gfx_FillRectangle((LCD_WIDTH - gfx_GetStringWidth(START_TEXT))/2, LCD_HEIGHT/2, gfx_GetStringWidth(START_TEXT), 16);
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
    uint16_t snakeCords[TOTAL_CORDS];
    uint16_t snakeLength = 3;
    uint16_t goalCords[2] = {120, 100}; // Starting goal
    uint16_t gameScore = 0;
    uint8_t snakeDirectionTimer = 0; // 0 = right, 1 = left, 2 = up, 3 = down
    uint8_t snakeDirectionLoop = 0; // 0 = right, 1 = left, 2 = up, 3 = down

    // Set start cords
    snakeCords[0] = 808;
    snakeCords[1] = 708;
    snakeCords[2] = 608;

    // Set pause bools
    enterPrevkey = false;
    isPaused = true;

    // Clear home screen again and make screen black
    os_ClrHome();
    gfx_FillScreen(BLACK_COLOR);

    // Snake movement timer variables
    timer_Control = TIMER1_DISABLE; // Disable the timer so it doesn't run when we don't want it to be running
    timer_1_ReloadValue = timer_1_Counter = SIXTH_SECOND; // By using the 32768 kHz clock, we can count for exactly 1 second here, or a different interval of time
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN; // Enable the timer, set it to the 32768 kHz clock, enable an interrupt once it reaches 0, and make it count down

    // Loop until Clear is pressed
    while (true) {
        // Snake movement timer
        if (timer_IntStatus & TIMER1_RELOADED) {
            uint16_t newHead;
            uint16_t headCordsX = (uint16_t)(snakeCords[0] / 100)*SQUARE_SIZE;
            uint16_t headCordsY = (uint16_t)(snakeCords[0] % 100)*SQUARE_SIZE;
            uint16_t tailCordsX = (uint16_t)(snakeCords[snakeLength-1] / 100)*SQUARE_SIZE;
            uint16_t tailCordsY = (uint16_t)(snakeCords[snakeLength-1] % 100)*SQUARE_SIZE;
            char scoreCounter[15];
            sprintf(scoreCounter, "Score: %i", gameScore);
            
            // Check head boundaries
            if (
                ((headCordsX) >= LCD_WIDTH)     ||  // Right out of bounds (Cords cannot be bigger)
                ((headCordsY) >= LCD_HEIGHT)    ||  // Bottom out of bounds (Cords cannot be bigger)
                ((headCordsX) < 0)              ||  // Left out of bounds (Cords can be equal to)
                ((headCordsY) < TOP_SAFESPACE)      // Top out of bounds (Cords can be equal to)
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
                // Increment game score
                gameScore = gameScore + 1;

                // Increment snake length
                snakeLength = snakeLength + 1;

                // Look for a new goal thats not on the snake
                bool newCoordAccepted = false;
                while (!newCoordAccepted) {
                    // Get new location
                    uint16_t newCordX = (rand() % WIDTH_TOTAL_CORDS)*SQUARE_SIZE; // Random number in safe space that is multiple of square size
                    uint16_t newCordY = (rand() % HEIGHT_TOTAL_CORDS)*SQUARE_SIZE+TOP_SAFESPACE; // Random number in safe space that is multiple of square size

                    // Check if the new coord is on the snake
                    bool notInList = true;
                    for (uint16_t i = 0; (i < snakeLength) && notInList; i++) {
                        // If the new location is on the snake stop looking
                        if ((newCordX == (uint16_t)(snakeCords[i] / 100)*SQUARE_SIZE) &&
                            (newCordY == (uint16_t)(snakeCords[i] % 100)*SQUARE_SIZE)) {
                            notInList = false;
                        }
                    }

                    // If the new coord is not in the list
                    // Set as new goal and stop the while loop
                    if (notInList) {
                        goalCords[0] = newCordX;
                        goalCords[1] = newCordY;
                        newCoordAccepted = true;
                    }
                }
            }

            // Set goal
            gfx_SetColor(RED_COLOR);
            gfx_FillRectangle(goalCords[0], goalCords[1], SQUARE_SIZE, SQUARE_SIZE);

            // Clear tail
            gfx_SetColor(BLACK_COLOR);
            gfx_FillRectangle(tailCordsX, tailCordsY, SQUARE_SIZE, SQUARE_SIZE);

            // Move the snake
            snakeDirectionTimer = snakeDirectionLoop;
            if (snakeDirectionTimer == 0) {
                // Move right
                newHead = ((headCordsX/SQUARE_SIZE)+1)*100+((headCordsY/SQUARE_SIZE));
            } else if (snakeDirectionTimer == 1) {
                // Move left
                newHead = ((headCordsX/SQUARE_SIZE)-1)*100+((headCordsY/SQUARE_SIZE));
            } else if (snakeDirectionTimer == 2) {
                // Move up
                newHead = ((headCordsX/SQUARE_SIZE))*100+((headCordsY/SQUARE_SIZE)-1);
            } else if (snakeDirectionTimer == 3) {
                // Move down
                newHead = ((headCordsX/SQUARE_SIZE))*100+((headCordsY/SQUARE_SIZE)+1);
            }

            // Loop through snake
            for (uint16_t i = snakeLength-1; i > 0; i--) {
                // If new head exists in snake then kill player
                if (snakeCords[i] == newHead) {
                    // Disable timer
                    timer_Control = TIMER1_DISABLE;
                    // Kill the player
                    dieScreen(gameScore);
                    // Break loop
                    break;
                }

                // Shift down the array
                snakeCords[i] = snakeCords[i-1];
            }
            
            // Add new head to array
            snakeCords[0] = newHead;

            // Update headCords variables
            headCordsX = (uint16_t)(snakeCords[0] / 100)*SQUARE_SIZE;
            headCordsY = (uint16_t)(snakeCords[0] % 100)*SQUARE_SIZE;

            // Set head
            gfx_SetColor(GREEN_COLOR);
            gfx_FillRectangle(headCordsX, headCordsY, SQUARE_SIZE, SQUARE_SIZE);

            // Show score
            gfx_SetColor(BLACK_COLOR);
            gfx_FillRectangle(0, 0, LCD_WIDTH, TOP_SAFESPACE-1);
            gfx_SetTextScale(2, 2); // Text size
            gfx_SetTextFGColor(WHITE_COLOR); // Text color
            gfx_PrintStringXY(scoreCounter, 0, 0); // Print text

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
                    if (snakeDirectionTimer != 1) {
                        snakeDirectionLoop = 0;
                    }
                    break;

                case kb_Left:
                    // If not coming from right, set to left
                    if (snakeDirectionTimer != 0) {
                        snakeDirectionLoop = 1;
                    }
                    break;

                case kb_Up:
                    // If not coming from down, set to up
                    if (snakeDirectionTimer != 3) {
                        snakeDirectionLoop = 2;
                    }
                    break;

                case kb_Down:
                    // If not coming from down, set to up
                    if (snakeDirectionTimer != 2) {
                        snakeDirectionLoop = 3;
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
                int i;

                // Clear the screen
                gfx_FillScreen(BLACK_COLOR);

                // Redraw body
                gfx_SetColor(GREEN_COLOR);
                for (i = 0; i < snakeLength; i++) {
                    gfx_FillRectangle((int)(snakeCords[i] / 100)*SQUARE_SIZE, (int)(snakeCords[i] % 100)*SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE);
                }
                
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

void dieScreen(uint16_t gameScore) {
    // Variables
    char highScoreMSG[18];
    sprintf(highScoreMSG, "High Score: %i", handleHighScore(gameScore));

    char scoreMSG[18];
    sprintf(scoreMSG, "Final Score: %i", gameScore);
    
    // Set text color
    gfx_SetTextFGColor(WHITE_COLOR); // Text color

    // Clear the screen
    gfx_FillScreen(BLACK_COLOR);

    // Show death message
    gfx_SetTextScale(3, 3);
    gfx_PrintStringXY(DEATH_MESSAGE, (LCD_WIDTH - gfx_GetStringWidth(DEATH_MESSAGE))/2, LCD_HEIGHT/8); // Print text
    
    // Show play again
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY(PLAY_AGAIN_MSG, (LCD_WIDTH - gfx_GetStringWidth(PLAY_AGAIN_MSG))/2, (LCD_HEIGHT*5)/16);
    
    // Show exit game
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY(EXIT_MSG, (LCD_WIDTH - gfx_GetStringWidth(EXIT_MSG))/2, (LCD_HEIGHT*7)/16);

    // Show high score
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY(highScoreMSG, (LCD_WIDTH - gfx_GetStringWidth(scoreMSG))/2, (LCD_HEIGHT*5)/8);

    // Show score
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY(scoreMSG, (LCD_WIDTH - gfx_GetStringWidth(scoreMSG))/2, (LCD_HEIGHT*3)/4); // Print text

    while (true) {
        // Update kb_Data
        kb_Scan();

        // Exit on clear or enter
        if (kb_Data[6] == kb_Clear){
            // Close the graphics
            gfx_End();
            break;
        } else if (kb_Data[6] == kb_Enter) {
            startGame();
            break;
        }
        
    }
}

// TODO High score handler, currently unreliable and can cause crashing
uint16_t handleHighScore(uint16_t score) {
    // High score
    uint16_t highScore = 0;

    // Get high score from file
    ti_var_t HSFile;
    HSFile = ti_Open("SnakeHS", "r+");
    if(HSFile) {
        ti_Read(&highScore, sizeof(uint8_t), sizeof(highScore)/sizeof(uint8_t), HSFile);
        ti_Close(HSFile);
    }


    // Check if current score is bigger
    if (score > highScore) {
        highScore = score;
    }

    // Write highScore to file
    HSFile = ti_Open("SnakeHS", "w");
    if(HSFile) {
        ti_Write(&highScore, sizeof(uint8_t), sizeof(highScore)/sizeof(uint8_t), HSFile);
    }

    // Close high score file
    if (HSFile) {
        // ti_SetArchiveStatus(true, HSFile);
        ti_Close(HSFile);
    }

    return highScore;       
}