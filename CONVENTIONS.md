# Project Conventions

## Code Style
*   **Indentation:** 2 spaces. No tabs.
*   **Braces:** K&R style (opening brace on the same line).
*   **Standard:** C++20.

## Header Files
*   **Include Guards:** Use `#ifndef`, `#define`, `#endif` (NO `#pragma once`).
*   **Structure:**
    1.  Class Declaration (Member variables & function prototypes).
    2.  **Inline Implementations:** Defined *outside* the class body at the bottom of the file.
    3.  **Ordering of Inlines:**
        *   Constructors
        *   Destructor
        *   Member Functions (sorted Alphabetically)
*   **Keywords:** Use the `inline` keyword explicitly for these implementations.