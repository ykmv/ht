#!/bin/sh

export HTDIR="./test"

RUN="valgrind --leak-check=full ./ht"

$RUN -h 2>>test.valgrind.log
$RUN -a test_habit 2>>test.valgrind.log
$RUN -s test_habit 2>>test.valgrind.log
$RUN 2>>test.valgrind.log
$RUN 2>>test.valgrind.log
$RUN 2>>test.valgrind.log
$RUN 2025.02.01 2>>test.valgrind.log
$RUN 02.02 2>>test.valgrind.log
$RUN 1 2>>test.valgrind.log
$RUN -cl 2>>test.valgrind.log
$RUN -c 2>>test.valgrind.log
$RUN -s 2>>test.valgrind.log
$RUN -s 2>>test.valgrind.log
$RUN -c fjasdklfjsdaf 2>>test.valgrind.log
$RUN -cl fjasdklfjsdaf 2>>test.valgrind.log
$RUN -H test_habit 2>>test.valgrind.log
$RUN -H test_habit 2025.03.01 2>>test.valgrind.log
$RUN -H test_habit 03.02 2>>test.valgrind.log
$RUN -H test_habit 4 2>>test.valgrind.log
$RUN -H test_habit 9999.99.99 2>>test.valgrind.log
$RUN -H test_habit -1000.-10.-10 2>>test.valgrind.log
$RUN -H test_habit 0.0.0 2>>test.valgrind.log
$RUN -c 2>>test.valgrind.log
$RUN -cl 2>>test.valgrind.log
$RUN -a t 2>>test.valgrind.log
$RUN -a t2 2>>test.valgrind.log
$RUN -a t3 2>>test.valgrind.log
$RUN -a  "あたまがわるい　仕事　イクラ проверка ЁÄÜÖ" 2>>test.valgrind.log
$RUN -H t 2>>test.valgrind.log
$RUN -H t2 2>>test.valgrind.log
$RUN -H t3 2>>test.valgrind.log
$RUN -H "あたまがわるい　仕事　イクラ проверка ЁÄÜÖ" 2>>test.valgrind.log
$RUN -l 2>>test.valgrind.log
$RUN -r  2>>test.valgrind.log
$RUN -r test_habit 2>>test.valgrind.log
$RUN -r test_habit 2>>test.valgrind.log

unset HTDIR
