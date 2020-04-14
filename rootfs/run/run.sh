#!/bin/sh

gpioset gpiochip2 3=1
stty -F /dev/ttySTM1 -echo -onlcr


