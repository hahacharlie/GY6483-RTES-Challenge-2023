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
#include <button.h>

Button::Button(PinName pin, PinMode mode) : ButtonInput(pin, mode)
{
    // click starts with rising edge, ends with falling edge
    ButtonInput.rise(callback(this, &Button::onButtonDown));
    ButtonInput.fall(callback(this, &Button::onButtonRelease));
}

void Button::onButtonDown()
{
    // start timer to measure how long the Button is pressed
    pressTimer.start();
    ButtonPressed = true;
}

void Button::onButtonRelease()
{
    if (pressTimer.read_ms() >= LONG_PRESS_TIME)
    {
        // long press
        onLongClickHandler();
    }
    else
    {
        // click

        if (previousClick)
        {
            // previos click within double click time span -> its a double click
            onDoubleClickHandler();

            // stop timeout
            doubleClickTimeout.detach();
            previousClick = false;
        }
        else
        {
            //onClickHandler();

            // start double click timer
            previousClick = true;
            doubleClickTimeout.attach_us(callback(this, &Button::checkDoubleClick), DOUBLE_CLICK_DELAY * 1000);
        }
    }

    // stop and reset timer
    pressTimer.stop();
    pressTimer.reset();
    ButtonPressed = false;
}

void Button::checkDoubleClick()
{
    // no second click in timeout, it was a single click
    onClickHandler();

    previousClick = false;
}
