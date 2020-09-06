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

#define TRANSPARENT_COLOR 10 // Some random transparent color
#define ONE_SECOND        32768/1 // One second on the timer
#define HALF_SECOND       32768/2 // Half a second on the timer
#define QUARTER_SECOND    32768/4 // A quarter second on the timer

void main(void) {
    homeScreen(); // Load the home screen
}

void homeScreen() {
    // Variables
    kb_key_t key;
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
    gfx_FillScreen(gfx_black);

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
    gfx_SetTextFGColor(255); // Text color
    gfx_PrintStringXY(startText, (LCD_WIDTH - gfx_GetStringWidth(startText))/2, LCD_HEIGHT/2); // Print text

    // Printing "Made by Banakin"
    gfx_SetTextScale(1, 1); // Text size
    gfx_SetTextFGColor(255); // Text color
    gfx_PrintStringXY(madeBy, (LCD_WIDTH - gfx_GetStringWidth(madeBy))/2, LCD_HEIGHT - 18); // Print text

    // Blinking text timer variables
    timer_Control = TIMER1_DISABLE; // Disable the timer so it doesn't run when we don't want it to be running
    timer_1_ReloadValue = timer_1_Counter = HALF_SECOND; // By using the 32768 kHz clock, we can count for exactly 1 second here, or a different interval of time
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN; // Enable the timer, set it to the 32768 kHz clock, enable an interrupt once it reaches 0, and make it count down
    
    // Loop until Clear is pressed
    do {
        // Blinking text timer
        if (timer_IntStatus & TIMER1_RELOADED) {
            if (startGameBlink == false) {
                // Print a string
                gfx_SetTextScale(2, 2); // Text size
                gfx_SetTextFGColor(255); // Text color
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
        }

        if (kb_Data[6] == kb_Clear){
            // Stop the timer
            timer_Control = TIMER1_DISABLE;
            // Clear the sprite from memory
            free(snake);
            // Close the graphics
            gfx_End();
        }
    } while (kb_Data[6] != kb_Clear && kb_Data[6] != kb_Enter);
}

void startGame() {
    // Variables
    bool key, prevkey;
    const char *pausedMessage = "Paused";
    bool isPaused = false;

    // Clear screen (Set it to black)
    gfx_SetColor(0);
    gfx_FillRectangle(0, 0, LCD_WIDTH, LCD_HEIGHT);

    // Snake movement timer variables
    timer_Control = TIMER1_DISABLE; // Disable the timer so it doesn't run when we don't want it to be running
    timer_1_ReloadValue = timer_1_Counter = QUARTER_SECOND; // By using the 32768 kHz clock, we can count for exactly 1 second here, or a different interval of time
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN; // Enable the timer, set it to the 32768 kHz clock, enable an interrupt once it reaches 0, and make it count down

    // Loop until Clear is pressed
    do {
        // Snake movement timer
        if (timer_IntStatus & TIMER1_RELOADED) {
            gfx_SetColor(6);
            gfx_FillRectangle(80, 80, 10, 10);
            // Acknowledge the reload
            timer_IntAcknowledge = TIMER1_RELOADED;
        }

        // Update kb_Data
        kb_Scan();

        if (kb_Data[6] == kb_Enter && !prevkey){
            if (isPaused == false){
                // Disable the timer
                timer_Control = TIMER1_DISABLE;
                // Set the game to paused
                isPaused = true;
                // Clear the screen
                gfx_SetColor(0);
                gfx_FillRectangle(0, 0, LCD_WIDTH, LCD_HEIGHT);
                // Set show paused screen
                // Printing "GAME GOES HERE LOL"
                gfx_SetTextScale(2, 2); // Text size
                gfx_SetTextFGColor(255); // Text color
                gfx_PrintStringXY(pausedMessage, (LCD_WIDTH - gfx_GetStringWidth(pausedMessage))/2, LCD_HEIGHT/2); // Print text
            } else if (isPaused == true){
                // Clear the screen
                gfx_SetColor(0);
                gfx_FillRectangle(0, 0, LCD_WIDTH, LCD_HEIGHT);
                // Re-enable timer
                timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN;
                // Set the game to paused
                isPaused = false;
            }
            prevkey = key;
        }

        if (kb_Data[6] == kb_Clear){
            // Close the graphics
            gfx_End();
        }
    } while (kb_Data[6] != kb_Clear /* && kb_Data[6] != kb_Enter */);
}