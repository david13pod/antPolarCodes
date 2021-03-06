# Copyright 2018 Florian Lotze
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

add_library(Modulator OBJECT
        modulation/modem
        modulation/bpsk
        modulation/ask
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/modulation/modem.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/modulation/bpsk.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/modulation/ask.h)

add_library(Transmitter OBJECT
        transmission/transmitter
        transmission/scale
        transmission/awgn
        transmission/rayleigh
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/transmission/transmitter.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/transmission/scale.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/transmission/awgn.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/transmission/rayleigh.h)

add_library(SignalProcessing
        random
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/avx_mathfun.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/lcg.h
        ${CMAKE_SOURCE_DIR}/include/signalprocessing/random.h
        $<TARGET_OBJECTS:Modulator>
        $<TARGET_OBJECTS:Transmitter>)
