/*
MIT License

Copyright (c) 2021 Haoran Wang.
Copyright (c) 2020 Steffen S.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <mbed.h>
#include <debounced_int.h>

#define LONG_PRESS_TIME 500
#define DOUBLE_CLICK_DELAY 250

// Button class with debounce and multiple actions
class Button
{
public:
    Button(PinName pin, PinMode mode = PullNone);

    void onClick(Callback<void()> clickHandler) { onClickHandler = clickHandler; };

    void onLongClick(Callback<void()> longHandler) { onLongClickHandler = longHandler; };

    void onDoubleClick(Callback<void()> doubleHandler) { onDoubleClickHandler = doubleHandler; };

private:
    DebouncedInt ButtonInput;
    Timer pressTimer;
    Timeout doubleClickTimeout;
    bool ButtonPressed;
    bool previousClick;
    Callback<void()> onClickHandler;
    Callback<void()> onDoubleClickHandler;
    Callback<void()> onLongClickHandler;

    void checkDoubleClick();
    void onButtonDown();
    void onButtonRelease();
};