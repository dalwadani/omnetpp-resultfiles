/*
 * Copyright (c) 2010, Andras Varga and Opensim Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Opensim Ltd. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Andras Varga or Opensim Ltd. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdlib.h>
#include "channel.h"
#include "xyplotnode.h"

USING_NAMESPACE


Port *XYPlotNode::getPortY(int k)
{
    if (k<0 || yin.size()< (unsigned)k)
        throw opp_runtime_error("XYPlotNode::getPortY(k): k=%d out of range, size of yin[] is %d",k,yin.size());
    if (yin.size()==(unsigned)k)
    {
        yin.push_back(Port(this));
        out.push_back(Port(this));
    }
    return &yin[k];
}

Port *XYPlotNode::getPortOut(int k)
{
    if (k<0 || out.size()<=(unsigned)k)
        throw opp_runtime_error("XYPlotNode::getPortOut(k): k=%d out of range, size of out[] is %d",k,out.size());
    return &out[k];
}

bool XYPlotNode::isReady() const
{
    // if no "x" data, we're not ready
    if (xin()->length()==0)
        return false;
    // otherwise, we're ready only if all "y" ports have something (except those at EOF)
    for (PortVector::const_iterator it=yin.begin(); it!=yin.end(); it++)
        if ((*it)()->length()==0 && !(*it)()->isClosing())
            return false;
    return true;
}

void XYPlotNode::process()
{
    // read one value from "x" port, then read "y" ports until their peek()'s
    // return ty>tx values (where tx is timestamp of the "x" value). Meanwhile,
    // if we find ty==tx values, output the (x,y) pair. If a port has reached
    // EOF, skip it.
    Assert(xin()->length()>0);
    Datum xd;
    xin()->read(&xd,1);

    for (unsigned int i=0; i<yin.size(); i++)
    {
        Channel *ychan = yin[i]();
        Assert(ychan->eof() || ychan->length()>0);
        const Datum *yp;
        Datum d;
        while ((yp=ychan->peek())!=NULL && yp->x < xd.x)
        {
            ychan->read(&d,1);  // discard if timestamps not equal
        }
        while ((yp=ychan->peek())!=NULL && yp->x == xd.x)
        {
            ychan->read(&d,1);
            d.x = xd.y;
            out[i]()->write(&d,1);
        }
        if (ychan->eof())
        {
            out[i]()->close(); // doesn't hurt
        }
    }
}

bool XYPlotNode::isFinished() const
{
    // if "x" reached eof, we're finished in any case
    if (xin()->eof())
        return true;
    // otherwise only if all "y" ports are at EOF
    for (PortVector::const_iterator it=yin.begin(); it!=yin.end(); it++)
        if (!(*it)()->eof())
            return false;
    return true;
}

//-------

const char *XYPlotNodeType::getDescription() const
{
    return "Creates x-y plot by matching values that arrive on one x and several\n"
           "y ports by their timestamps. (One value on x and one on y create one\n"
           "output value when their timestamps are equal).";
}

void XYPlotNodeType::getAttributes(StringMap& attrs) const
{
}

Node *XYPlotNodeType::create(DataflowManager *mgr, StringMap& attrs) const
{
    checkAttrNames(attrs);

    Node *node = new XYPlotNode();
    node->setNodeType(this);
    mgr->addNode(node);
    return node;
}

Port *XYPlotNodeType::getPort(Node *node, const char *portname) const
{
    XYPlotNode *node1 = dynamic_cast<XYPlotNode *>(node);
    if (!strcmp(portname,"x"))
        return &(node1->xin);
    if (portname[0]=='y')
        return node1->getPortY(atoi(portname+1));
    else if (!strncmp(portname,"out",3))
        return node1->getPortOut(atoi(portname+3));
    throw opp_runtime_error("no such port `%s'", portname);
}



