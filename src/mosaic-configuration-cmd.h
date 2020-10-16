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
#ifndef SRC_MOSAIC_MGMT_MOSAICCONFIGURATIONCMD_H_
#define SRC_MOSAIC_MGMT_MOSAICCONFIGURATIONCMD_H_

#include <stdint.h>

class MosaicConfigurationCmd {
public:
    MosaicConfigurationCmd();
    virtual ~MosaicConfigurationCmd();
    int getChannel00() const;
    void setChannel00(int channel00Var);
    int getChannel01() const;
    void setChannel01(int channel01Var);
    int getChannel10() const;
    void setChannel10(int channel10Var);
    int getChannel11() const;
    void setChannel11(int channel11Var);
    uint8_t* getIp0();
    uint8_t* getIp1();
    int getMsgId() const;
    void setMsgId(int msgIdVar);
    int getNodeId() const;
    void setNodeId(int nodeIdVar);
    int getNumchannels0() const;
    void setNumchannels0(int numchannels0Var);
    int getNumchannels1() const;
    void setNumchannels1(int numchannels1);
    int getNumRadios() const;
    void setNumRadios(int numRadios);
    int getPower0() const;
    void setPower0(int power0);
    int getPower1() const;
    void setPower1(int power1);
    uint8_t* getSubnet0();
    uint8_t* getSubnet1();
    bool isTurnedOn0() const;
    void setTurnedOn0(bool turnedOn0);
    bool isTurnedOn1() const;
    void setTurnedOn1(bool turnedOn1);
    long getTime() const;
    void setTime(long time);

protected:
    long time;
    int msgId_var;
    int nodeId_var;
    int numRadios_var;
    bool turnedOn0_var;
    uint8_t ip0_var[4];
    uint8_t subnet0_var[4];
    int power0_var;
    int numchannels0_var;
    int channel00_var;
    int channel01_var;
    bool turnedOn1_var;
    uint8_t ip1_var[4];
    uint8_t subnet1_var[4];
    int power1_var;
    int numchannels1_var;
    int channel10_var;
    int channel11_var;
};

#endif /* SRC_MOSAIC_MGMT_MOSAICCONFIGURATIONCMD_H_ */
