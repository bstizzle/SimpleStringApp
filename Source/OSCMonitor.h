/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             OSCMonitorTutorial
 version:          2.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Monitors incoming OSC messages.

 dependencies:     juce_core, juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_osc
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once
#include <JuceHeader.h>

//==============================================================================
class OSCLogListBox : public juce::ListBox,
    private juce::ListBoxModel,
    private juce::AsyncUpdater
{
public:
    //==============================================================================
    OSCLogListBox()
    {
        setModel(this);
    }

    //==============================================================================
    int getNumRows() override
    {
        return oscLogList.size();
    }

    //==============================================================================
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        juce::ignoreUnused(rowIsSelected);

        if (juce::isPositiveAndBelow(row, oscLogList.size()))
        {
            g.setColour(juce::Colours::white);

            g.drawText(oscLogList[row],
                juce::Rectangle<int>(width, height).reduced(4, 0),
                juce::Justification::centredLeft, true);
        }
    }

    //==============================================================================
    void addOSCMessage(const juce::OSCMessage& message, int level = 0)
    {
        oscLogList.add(getIndentationString(level)
            + "- osc message, address = '"
            + message.getAddressPattern().toString()
            + "', "
            + juce::String(message.size())
            + " argument(s)");

        if (!message.isEmpty())
        {
            for (auto* arg = message.begin(); arg != message.end(); ++arg)
                addOSCMessageArgument(*arg, level + 1);
        }

        triggerAsyncUpdate();
    }

    //==============================================================================
    void addOSCBundle(const juce::OSCBundle& bundle, int level = 0)
    {
        juce::OSCTimeTag timeTag = bundle.getTimeTag();

        oscLogList.add(getIndentationString(level)
            + "- osc bundle, time tag = "
            + timeTag.toTime().toString(true, true, true, true));

        for (auto* element = bundle.begin(); element != bundle.end(); ++element)
        {
            if (element->isMessage())
                addOSCMessage(element->getMessage(), level + 1);
            else if (element->isBundle())
                addOSCBundle(element->getBundle(), level + 1);
        }

        triggerAsyncUpdate();
    }

    //==============================================================================
    void addOSCMessageArgument(const juce::OSCArgument& arg, int level)
    {
        juce::String typeAsString;
        juce::String valueAsString;

        if (arg.isFloat32())
        {
            typeAsString = "float32";
            valueAsString = juce::String(arg.getFloat32());
        }
        else if (arg.isInt32())
        {
            typeAsString = "int32";
            valueAsString = juce::String(arg.getInt32());
        }
        else if (arg.isString())
        {
            typeAsString = "string";
            valueAsString = arg.getString();
        }
        else if (arg.isBlob())
        {
            typeAsString = "blob";
            auto& blob = arg.getBlob();
            valueAsString = juce::String::fromUTF8((const char*)blob.getData(), (int)blob.getSize());
        }
        else
        {
            typeAsString = "(unknown)";
        }

        oscLogList.add(getIndentationString(level + 1) + "- " + typeAsString.paddedRight(' ', 12) + valueAsString);
    }

    //==============================================================================
    void addInvalidOSCPacket(const char* /* data */, int dataSize)
    {
        oscLogList.add("- (" + juce::String(dataSize) + "bytes with invalid format)");
    }

    //==============================================================================
    void clear()
    {
        oscLogList.clear();
        triggerAsyncUpdate();
    }

    //==============================================================================
    void handleAsyncUpdate() override
    {
        updateContent();
        scrollToEnsureRowIsOnscreen(oscLogList.size() - 1);
        repaint();
    }

private:
    //==============================================================================
    juce::String getIndentationString(int level)
    {
        return juce::String().paddedRight(' ', 2 * level);
    }

    //==============================================================================
    juce::StringArray oscLogList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCLogListBox)
};

//==============================================================================
class MainContentComponent : public juce::Component,
    private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback> // [1]
{
public:
    //==============================================================================
    MainContentComponent()
    {
        portNumberLabel.setBounds(10, 18, 130, 25);
        addAndMakeVisible(portNumberLabel);

        portNumberField.setEditable(true, true, true);
        portNumberField.setBounds(140, 18, 50, 25);
        addAndMakeVisible(portNumberField);

        connectButton.setBounds(210, 18, 100, 25);
        addAndMakeVisible(connectButton);
        connectButton.onClick = [this] { connectButtonClicked(); };

        clearButton.setBounds(320, 18, 60, 25);
        addAndMakeVisible(clearButton);
        clearButton.onClick = [this] { clearButtonClicked(); };

        connectionStatusLabel.setBounds(450, 18, 240, 25);
        updateConnectionStatusLabel();
        addAndMakeVisible(connectionStatusLabel);

        oscLogListBox.setBounds(0, 60, 700, 340);  // [4]
        addAndMakeVisible(oscLogListBox);

        oscReceiver.addListener(this);             // [5]
        oscReceiver.registerFormatErrorHandler([this](const char* data, int dataSize) // [6]
            {
                oscLogListBox.addInvalidOSCPacket(data, dataSize);
            });

        setSize(700, 400);
    }

private:
    //==============================================================================
    juce::Label portNumberLabel{ {}, "UDP Port Number: " };
    juce::Label portNumberField{ {}, "9002" };
    juce::TextButton connectButton{ "Connect" };
    juce::TextButton clearButton{ "Clear" };
    juce::Label connectionStatusLabel;

    OSCLogListBox oscLogListBox;        // [2]
    juce::OSCReceiver oscReceiver;

    int currentPortNumber = -1;         // [3]

    //==============================================================================
    void connectButtonClicked()
    {
        if (!isConnected())            // [7]
            connect();
        else
            disconnect();

        updateConnectionStatusLabel();  // [8]
    }

    //==============================================================================
    void clearButtonClicked()
    {
        oscLogListBox.clear();
    }

    //==============================================================================
    void oscMessageReceived(const juce::OSCMessage& message) override
    {
        oscLogListBox.addOSCMessage(message);
    }

    void oscBundleReceived(const juce::OSCBundle& bundle) override
    {
        oscLogListBox.addOSCBundle(bundle);
    }

    //==============================================================================
    void connect()
    {
        auto portToConnect = portNumberField.getText().getIntValue();   // [9]

        if (!isValidOscPort(portToConnect))                           // [10]
        {
            handleInvalidPortNumberEntered();
            return;
        }

        if (oscReceiver.connect(portToConnect))                        // [11]
        {
            currentPortNumber = portToConnect;
            connectButton.setButtonText("Disconnect");
        }
        else                                                            // [12]
        {
            handleConnectError(portToConnect);
        }
    }

    //==============================================================================
    void disconnect()
    {
        if (oscReceiver.disconnect())   // [13]
        {
            currentPortNumber = -1;
            connectButton.setButtonText("Connect");
        }
        else
        {
            handleDisconnectError();    // [14]
        }
    }

    //==============================================================================
    void handleConnectError(int failedPort)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "OSC Connection error",
            "Error: could not connect to port " + juce::String(failedPort),
            "OK");
    }

    //==============================================================================
    void handleDisconnectError()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Unknown error",
            "An unknown error occured while trying to disconnect from UDP port.",
            "OK");
    }

    //==============================================================================
    void handleInvalidPortNumberEntered()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Invalid port number",
            "Error: you have entered an invalid UDP port number.",
            "OK");
    }

    //==============================================================================
    bool isConnected() const
    {
        return currentPortNumber != -1;
    }

    //==============================================================================
    bool isValidOscPort(int port) const
    {
        return port > 0 && port < 65536;
    }

    //==============================================================================
    void updateConnectionStatusLabel()
    {
        juce::String text = "Status: ";

        if (isConnected())
            text += "Connected to UDP port " + juce::String(currentPortNumber);
        else
            text += "Disconnected";

        auto textColour = isConnected() ? juce::Colours::green : juce::Colours::red;

        connectionStatusLabel.setText(text, juce::dontSendNotification);
        connectionStatusLabel.setFont(juce::Font(15.00f, juce::Font::bold));
        connectionStatusLabel.setColour(juce::Label::textColourId, textColour);
        connectionStatusLabel.setJustificationType(juce::Justification::centredRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
