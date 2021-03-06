/*
 * Copyright 2000-2021. All rights reserved.
 * Distributed under the terms of the GPLv2 license.
 *
 * Author:
 *	Michael Pfeiffer et al.
 *  2021 Humdinger, humdingerb@gmail.com
 *
 */

#include "Keyboard2d.h"
#include "MsgConsts.h"
#include "View.h"

#include <Catalog.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <SplitView.h>

#include <ctype.h>
#include <algorithm>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Sliders"

// View
View::View(int16 octaves, int16 rows, BView* popView)
	:
	BView("options", B_WILL_DRAW | B_SUPPORTS_LAYOUT),
	BMidiLocalConsumer(APPLICATION " " VERSION)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	midiOut = new MidiOut(APPLICATION " " VERSION);
	midiOut->Register();
	Register();

	keys.remap = false;
	for (int i = 0; i < 128; i++) {
		notePlaying[i].device = 0;
		notePlaying[i].count = 0;
	}

	noteOffset = 12 * 3; // octave = 4
	channel = 0;
	chord.enabled = false;

	for (int i = 0; i < 16; i++) {
		channels[i].pressure = 127;
		channels[i].velocity = 64;
		channels[i].pitch = 64;
		channels[i].pan = 63;
		channels[i].instrument = 0;
	}

	// set interface elements
	pitchBendSlider = new BSlider("pitchBend", B_TRANSLATE("Pitch"),
		new BMessage(MSG_PITCH_BEND_CHANGED), 0, 127, B_VERTICAL);
	pitchBendSlider->SetHashMarks(B_HASH_MARKS_BOTH);
	pitchBendSlider->SetHashMarkCount(3);
	pitchBendSlider->SetValue(64);
	pitchBendSlider->SetLimitLabels(B_TRANSLATE("Low"), B_TRANSLATE("High"));
	pitchBendSlider->SetModificationMessage(
		new BMessage(MSG_PITCH_BEND_CHANGED));

	velocitySlider = new BSlider("velocitySlider", B_TRANSLATE("Velocity"),
		new BMessage(MSG_VELOCITY_CHANGED), 0, 127, B_VERTICAL);
	velocitySlider->SetHashMarks(B_HASH_MARKS_BOTH);
	velocitySlider->SetHashMarkCount(11);
	velocitySlider->SetValue(64);
	velocitySlider->SetLimitLabels(B_TRANSLATE("Min"), B_TRANSLATE("Max"));
	velocitySlider->SetModificationMessage(new BMessage(MSG_VELOCITY_CHANGED));

	volumeSlider = new BSlider("volumeSlider", B_TRANSLATE("Volume"),
		new BMessage(MSG_VOLUME_CHANGED), 0, 100, B_VERTICAL);
	volumeSlider->SetHashMarks(B_HASH_MARKS_BOTH);
	volumeSlider->SetHashMarkCount(11);
	volumeSlider->SetValue(100);
	volumeSlider->SetLimitLabels(B_TRANSLATE("Off"), B_TRANSLATE("Max"));
	volumeSlider->SetModificationMessage(new BMessage(MSG_VOLUME_CHANGED));

//#if 0
//	rect.SetLeftTop(BPoint(bounds.right, volumeSlider->Bounds().Height()+1));
//	chPressureSlider = new BSlider("chPressureSlider", "Channel Pressure",
//		new BMessage(MSG_CH_PRESSURE_CHANGED), 0, 127);
//	chPressureSlider->SetValue(127);
//	chPressureSlider->SetLimitLabels("Min", "Max");
//	chPressureSlider->SetModificationMessage(new BMessage(MSG_CH_PRESSURE_CHANGED));
//	AddChild(chPressureSlider);
//	chPressureSlider->ResizeTo(WIDTH*10.5, chPressureSlider->Bounds().Height());
//#else
	chPressureSlider = NULL;
//#endif
//#if 1
	panSlider = new BSlider(
		"panSlider", NULL, new BMessage(MSG_PAN_CHANGED), 0, 127, B_HORIZONTAL);
	panSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	panSlider->SetHashMarkCount(3);
	panSlider->SetValue(64);
	panSlider->SetLimitLabels(B_TRANSLATE("Left"), B_TRANSLATE("Right"));
	panSlider->SetModificationMessage(new BMessage(MSG_PAN_CHANGED));
//#else
//	panSlider = NULL;
//#endif
	Keyboard2D* k2d = new Keyboard2D(this, octaves, rows);
	keyboard = k2d;
	currentKeyboard = 0;

	BSplitView* splitView = new BSplitView(B_HORIZONTAL, B_USE_SMALL_SPACING);

	BLayoutBuilder::Group<>(this, B_HORIZONTAL)
		.AddSplit(splitView)
			.AddGroup(B_VERTICAL)
				.SetInsets(B_USE_WINDOW_SPACING, 0, 0, B_USE_WINDOW_SPACING)
				.Add(popView)
				.Add(new BSeparatorView(B_HORIZONTAL))
				.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
					.Add(pitchBendSlider)
					.Add(velocitySlider)
					.Add(volumeSlider)
					.End()
				.Add(new BSeparatorView(B_HORIZONTAL))
				.Add(panSlider)
				.End()
			.AddGroup(B_VERTICAL)
				.Add(k2d)
				.SetInsets(0, 0, 0, B_USE_WINDOW_SPACING)
				.End()
		.SetInsets(B_USE_SMALL_INSETS)
		.End();
}


View::~View()
{
}


void
View::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_MOUSE_WHEEL_CHANGED:
		{
			BPoint pt;
			uint32 buttons;
			GetMouse(&pt, &buttons, false);
			BView* wheelView = Window()->FindView(pt);
			if (wheelView == NULL)
				break;

			float deltaY = 0.0f;
			if (message->FindFloat("be:wheel_delta_y", &deltaY) != B_OK
				|| deltaY == 0.0f)
					break;

			if (pitchBendSlider->Frame().Contains(pt)) {
				// round to multiple of 8, or we won't hit the hash marks
				// if the slider knob was manually put between hash marks
				int pitch = (pitchBendSlider->Value() & (-8));
				if (deltaY < 0)
					std::min(pitch += 8, 128);
				else
					std::max(pitch -= 8, 0);

				pitchBendSlider->SetValue(pitch);
				Window()->PostMessage(MSG_PITCH_BEND_CHANGED);
				break;
			} else if (velocitySlider->Frame().Contains(pt)) {
				int velocity = (velocitySlider->Value() & (-8));;
				if (deltaY < 0)
					std::min(velocity += 8, 128);
				else
					std::max(velocity -= 8, 0);

				velocitySlider->SetValue(velocity);
				Window()->PostMessage(MSG_VELOCITY_CHANGED);
				break;
			} else if (volumeSlider->Frame().Contains(pt)) {
				int volume = volumeSlider->Value();
				int32 a = (volume / 10) * 10; // round to nearest multiple of 10
				int32 b = a + 10;
				volume = (volume - a > b - volume)? b : a;
				if (deltaY < 0)
					std::min(volume += 10, 100);
				else
					std::max(volume -= 10, 0);

				volumeSlider->SetValue(volume);
				Window()->PostMessage(MSG_VOLUME_CHANGED);
				break;
			} else if (panSlider->Frame().Contains(pt)) {
				int pan = (panSlider->Value() & (-8));
				if (deltaY < 0)
					std::min(pan += 8, 128);
				else
					std::max(pan -= 8, 0);

				panSlider->SetValue(pan);
				Window()->PostMessage(MSG_PAN_CHANGED);
				break;
			}
			break;
		}
		{
			default:
				BView::MessageReceived(message);
				break;
		}
	}
}


void
View::FocusKeyboard()
{
	((Keyboard2D*) keyboard)->MakeFocus(true);
}


void
View::SetKeyboard(int index)
{
	if (currentKeyboard != index) {
		if (currentKeyboard == 0)
			RemoveChild((Keyboard2D*) keyboard);
#if 0
		else
			RemoveChild((Keyboard3D*)keyboard);
#endif
		delete keyboard;
		currentKeyboard = index;
		if (currentKeyboard == 0) {
			Keyboard2D* k2d = new Keyboard2D(this, 3, 2);
			keyboard = k2d;
			AddChild(k2d);
		}
#if 0
	    else {
			BRect rect = Bounds();
			rect.top = subViewTop;
			Keyboard3D *k3d = new Keyboard3D(this, rect);
			keyboard = k3d;
			AddChild(k3d);
			k3d->Render();
		}
#endif
	}
}


Keyboard*
View::GetKeyboard()
{
	return (Keyboard2D*) keyboard;
}


void
View::SetChannel(int channel)
{
	this->channel = channel;
	if (Window()->Lock()) {
		pitchBendSlider->SetValue(GetPitchBend(channel));
		velocitySlider->SetValue(GetVelocity(channel));
		panSlider->SetValue(127 - GetPan(channel));
		Window()->Unlock();
	}
}


uchar
View::NotePlaying(uchar note)
{
	if (note > 127)
		return false;
	else
		return notePlaying[note].device;
}


void
View::SetOctave(uchar octave)
{
	if (noteOffset != octave * 12) {
		noteOffset = octave * 12;
		if (currentKeyboard == 0)
			((Keyboard2D*) keyboard)->Invalidate();
#if 0
		else
			((Keyboard3D*)keyboard)->Invalidate();
#endif
	}
};


void
View::ChannelPressureChanged()
{
	int p = chPressureSlider->Value();
	midiOut->SprayChannelPressure(channel, p, system_time());
	SetChannelPressure(channel, p);
}


void
View::PitchBendChanged()
{
	uchar pitch = pitchBendSlider->Value();
	midiOut->SprayPitchBend(channel, 0, pitch, system_time());
	SetPitchBend(channel, pitch);

	rgb_color high = { 0, 0, 255, 255 };
	rgb_color low = { 255, 0, 0, 255 };
	rgb_color bar = be_control_look->SliderBarColor(
		ui_color(B_PANEL_BACKGROUND_COLOR));

	if (pitch > 64)
		bar = mix_color(ui_color(B_PANEL_BACKGROUND_COLOR), high, pitch);
	else if (pitch < 64)
		bar = mix_color(ui_color(B_PANEL_BACKGROUND_COLOR), low, 128 - pitch);

	pitchBendSlider->SetBarColor(bar);
}


void
View::PanChanged()
{
	uchar pan = panSlider->Value();
	midiOut->SprayControlChange(channel, B_PAN, pan, system_time());
	SetPan(channel, pan);

	rgb_color left = { 0, 255, 0, 255 };
	rgb_color right = { 255, 0, 0, 255 };
	rgb_color bar = be_control_look->SliderBarColor(
		ui_color(B_PANEL_BACKGROUND_COLOR));

	if (pan > 64)
		bar = mix_color(ui_color(B_PANEL_BACKGROUND_COLOR), left, pan);
	else if (pan < 64)
		bar = mix_color(ui_color(B_PANEL_BACKGROUND_COLOR), right, 128 - pan);

	panSlider->SetBarColor(bar);
}


void
View::SetMainVolume(double vol)
{
	if (Window()->Lock()) {
		volumeSlider->SetValue((int32)(vol * 100.0));
		Window()->Unlock();
	}
}


double
View::GetMainVolume()
{
	return volumeSlider->Value() / 100.0;
}


void
View::VelocityChanged()
{
	SetVelocity(channel, velocitySlider->Value());
}


uchar
View::Key2Note(uchar key)
{
	if (key == 255)
		return 255;
	int note = key + noteOffset;
	if (note >= 128)
		return 255;
	if (note < 0)
		return 255;
	return note;
}


void
View::NoteOn(uchar key, uchar source, bool map)
{
	uchar note = (map) ? Key2Note(key) : key;
	if (note == 255)
		return;

	if ((keys.remap) && (source == MOUSE))
		keys.note = key;

	uchar old = notePlaying[note].device;
	notePlaying[note].device |= source;
	if (!old) {
		midiOut->SprayNoteOn(
			channel, note, channels[channel].velocity, system_time());
		keyboard->NoteOn(note);
	}

	if (chord.enabled && (old != notePlaying[note].device)) {
		uchar n;
		for (int i = 0; i < chord.len; i++) {
			n = note + chord.notes[i];
			if (n > 127)
				return;
			old = notePlaying[n].device;
			notePlaying[n].device |= CHORD;
			notePlaying[n].count++;
			if (!old) {
				midiOut->SprayNoteOn(
				channel, n, channels[channel].velocity, system_time());
				keyboard->NoteOn(n);
			}
		}
	}
}


void
View::NoteOff(uchar key, uchar source, bool map)
{
	uchar note = (map) ? Key2Note(key) : key;
	if (note == 255)
		return;
	uchar old = notePlaying[note].device;
	notePlaying[note].device &= 255 ^ source;
	if (!notePlaying[note].device) {
		midiOut->SprayNoteOff(
			channel, note, channels[channel].velocity, system_time());
		keyboard->NoteOff(note);
	}

	if (chord.enabled && (old != notePlaying[note].device)) {
		uchar n;
		for (int i = 0; i < chord.len; i++) {
			n = note + chord.notes[i];
			if (n > 127)
				return;

			if (!(notePlaying[n].device & CHORD))
				continue;

			notePlaying[n].count--;
			if (notePlaying[n].count == 0)
				notePlaying[n].device &= 255 ^ CHORD;
			if (!notePlaying[n].device) {
				midiOut->SprayNoteOff(
					channel, n, channels[channel].velocity, system_time());
				keyboard->NoteOff(n);
			}
		}
	}
}


void
View::KeyDown(int32 key, int32 modifiers)
{
	if (keys.remap && keyTable.SetKey(key, modifiers, keys.note))
		keys.note = 255;

	NoteOn(keyTable.Key2Note(key, modifiers), KEYBOARD, true);
}


void
View::KeyUp(int32 key, int32 modifier)
{
	NoteOff(keyTable.Key2Note(key, modifier), KEYBOARD, true);
}

// Key Table
KeyTable*
View::GetKeyTable()
{
	return &keyTable;
}


void
View::BeginRemapKeys()
{
	keys.note = 255;
	keys.remap = true;
}


void
View::EndRemapKeys()
{
	keys.remap = false;
}


void
View::ReleaseNotes(uchar source)
{
	for (int i = 0; i < 128; i++) {
		notePlaying[i].device &= 255 ^ source;
		notePlaying[i].count = 0;
		if (!notePlaying[i].device) {
			midiOut->SprayNoteOff(
				channel, i, channels[channel].velocity, system_time());
			keyboard->NoteOff(i);
		}
	}
}


void
View::AllNotesOff(uchar source)
{
	int i;
	for (i = 0; i < 128; i++)
		notePlaying[i].device &= 255 ^ source;
}


void
View::Invalidate()
{
	if (currentKeyboard == 0)
		((Keyboard2D*) keyboard)->Invalidate();
#if 0
	else
		((Keyboard3D*)keyboard)->Invalidate();
#endif
	BView::Invalidate();
}

// Chord
static uchar
DecodeChord(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	else
		return toupper(ch) - 'A' + 10;
}


void
View::SetChord(const char* chord)
{
	int i;
	ClearChord();

	for (i = 0; (i < 255) && (chord[i] != '\0'); i++) {
		if (i == 0)
			this->chord.notes[0] = DecodeChord(chord[0]);
		else
			this->chord.notes[i]
				= this->chord.notes[i - 1] + DecodeChord(chord[i]);
	}

	if (i > 0) {
		this->chord.len = i;
		this->chord.enabled = true;
	}
}


void
View::ClearChord()
{
	if (chord.enabled) {
		chord.enabled = false;
		ReleaseNotes(8);
	}
}

// Midi Input
#define ANIMATE_KEYS true


void
View::ChannelPressure(uchar channel, uchar pressure, bigtime_t time)
{
	midiOut->SprayChannelPressure(channel, pressure, time);
	if ((pressure != channels[channel].pressure))
		SetChannelPressure(channel, pressure);
	// if (ANIMATE_KEYS && (channel == this->channel) && Window()->Lock()) {
	//	chPressureSlider->SetValue(pressure);
	//	Window()->Unlock();
	//}
}


void
View::PitchBend(uchar channel, uchar lsb, uchar msb, bigtime_t time)
{
	midiOut->SprayPitchBend(channel, lsb, msb, time);
	if (msb != channels[channel].pitch) {
		SetPitchBend(channel, msb);
		if (ANIMATE_KEYS && (channel == this->channel) && Window()->Lock()) {
			pitchBendSlider->SetValue(msb);
			Window()->Unlock();
			Window()->PostMessage(MSG_PITCH_BEND_CHANGED);
		}
	}
}


void
View::NoteOn(uchar channel, uchar note, uchar velocity, bigtime_t time)
{
	if (ANIMATE_KEYS && (channel == this->channel)) {
		bool v = channels[channel].velocity != velocity;
		if (v)
			channels[channel].velocity = velocity;
		if (velocity == 0)
			NoteOff(note, MIDIIN, false);
		else
			NoteOn(note, MIDIIN, false);
		if (v && Window()->Lock()) {
			velocitySlider->SetValue(velocity);
			Window()->Unlock();
		}
	} else {
		SetVelocity(channel, velocity);
		midiOut->SprayNoteOn(channel, note, velocity, time);
	}
}


void
View::NoteOff(uchar channel, uchar note, uchar velocity, bigtime_t time)
{
	if (ANIMATE_KEYS && (channel == this->channel))
		NoteOff(note, MIDIIN, false);
	else
		midiOut->SprayNoteOff(channel, note, velocity, time);
}


void
View::ControlChange(
	uchar channel, uchar controlNumber, uchar controlValue, bigtime_t time)
{
	midiOut->SprayControlChange(channel, controlNumber, controlValue, time);
	if ((controlNumber == B_PAN) && (controlValue != channels[channel].pan)) {
		SetPan(channel, controlValue);
		if (ANIMATE_KEYS && (channel == this->channel) && Window()->Lock()) {
			panSlider->SetValue(127 - controlValue);
			Window()->Unlock();
			Window()->PostMessage(MSG_PAN_CHANGED);
		}
	}
}


void
View::ProgramChange(uchar channel, uchar progNum, bigtime_t time)
{
	midiOut->SprayProgramChange(channel, progNum, time);
	SetInstrument(channel, progNum);
}


void
View::AllNotesOff(bool justChannel, bigtime_t time)
{
	midiOut->AllNotesOff(channel, time);
}


void
View::KeyPressure(uchar channel, uchar note, uchar pressure, bigtime_t time)
{
	midiOut->SprayKeyPressure(channel, note, pressure, time);
}


void
View::SystemExclusive(void* data, size_t dataLength, bigtime_t time)
{
	midiOut->SpraySystemExclusive(data, dataLength, time);
}


void
View::SystemCommon(uchar statusByte, uchar data1, uchar data2, bigtime_t time)
{
	midiOut->SpraySystemCommon(statusByte, data1, data2, time);
}


void
View::SystemRealTime(uchar statusByte, bigtime_t time)
{
	midiOut->SpraySystemRealTime(statusByte, time);
}


void
View::TempoChange(int32 bpm, bigtime_t time)
{
	midiOut->SprayTempoChange(bpm, time);
}
