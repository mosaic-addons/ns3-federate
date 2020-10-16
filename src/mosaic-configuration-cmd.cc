/*
 * Copyright (c) 2020 Fraunhofer FOKUS and others. All rights reserved.
 *
 * Contact: mosaic@fokus.fraunhofer.de
 *
 * This class is developed for the MOSAIC-NS-3 coupling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "mosaic-configuration-cmd.h"

int MosaicConfigurationCmd::getChannel00() const {
    return channel00_var;
}

void MosaicConfigurationCmd::setChannel00(int channel00Var) {
    channel00_var = channel00Var;
}

int MosaicConfigurationCmd::getChannel01() const {
    return channel01_var;
}

void MosaicConfigurationCmd::setChannel01(int channel01Var) {
    channel01_var = channel01Var;
}

int MosaicConfigurationCmd::getChannel10() const {
    return channel10_var;
}

void MosaicConfigurationCmd::setChannel10(int channel10Var) {
    channel10_var = channel10Var;
}

int MosaicConfigurationCmd::getChannel11() const {
    return channel11_var;
}

void MosaicConfigurationCmd::setChannel11(int channel11Var) {
    channel11_var = channel11Var;
}

uint8_t* MosaicConfigurationCmd::getIp0() {
    return ip0_var;
}

uint8_t* MosaicConfigurationCmd::getIp1() {
    return ip1_var;
}

int MosaicConfigurationCmd::getMsgId() const {
    return msgId_var;
}

void MosaicConfigurationCmd::setMsgId(int msgIdVar) {
    msgId_var = msgIdVar;
}

int MosaicConfigurationCmd::getNodeId() const {
    return nodeId_var;
}

void MosaicConfigurationCmd::setNodeId(int nodeIdVar) {
    nodeId_var = nodeIdVar;
}

int MosaicConfigurationCmd::getNumchannels0() const {
    return numchannels0_var;
}

void MosaicConfigurationCmd::setNumchannels0(int numchannels0Var) {
    numchannels0_var = numchannels0Var;
}

int MosaicConfigurationCmd::getNumchannels1() const {
    return numchannels1_var;
}

void MosaicConfigurationCmd::setNumchannels1(int numchannels1Var) {
    numchannels1_var = numchannels1Var;
}

int MosaicConfigurationCmd::getNumRadios() const {
    return numRadios_var;
}

void MosaicConfigurationCmd::setNumRadios(int numRadiosVar) {
    numRadios_var = numRadiosVar;
}

int MosaicConfigurationCmd::getPower0() const {
    return power0_var;
}

void MosaicConfigurationCmd::setPower0(int power0Var) {
    power0_var = power0Var;
}

int MosaicConfigurationCmd::getPower1() const {
    return power1_var;
}

void MosaicConfigurationCmd::setPower1(int power1Var) {
    power1_var = power1Var;
}

uint8_t* MosaicConfigurationCmd::getSubnet0() {
    return subnet0_var;
}

uint8_t* MosaicConfigurationCmd::getSubnet1() {
    return subnet1_var;
}

bool MosaicConfigurationCmd::isTurnedOn0() const {
    return turnedOn0_var;
}

void MosaicConfigurationCmd::setTurnedOn0(bool turnedOn0Var) {
    turnedOn0_var = turnedOn0Var;
}

bool MosaicConfigurationCmd::isTurnedOn1() const {
    return turnedOn1_var;
}

void MosaicConfigurationCmd::setTurnedOn1(bool turnedOn1Var) {
    turnedOn1_var = turnedOn1Var;
}

long MosaicConfigurationCmd::getTime() const {
    return time;
}

void MosaicConfigurationCmd::setTime(long time) {
    this->time = time;
}
