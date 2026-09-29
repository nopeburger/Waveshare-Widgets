#pragma once
#if __has_include("widget_selection.h")
#include "widget_selection.h"
#endif
#ifndef DECK_ENABLE_FLIGHT
#define DECK_ENABLE_FLIGHT 1
#endif
#ifndef DECK_ENABLE_GAS
#define DECK_ENABLE_GAS 1
#endif
#ifndef DECK_ENABLE_EARTHQUAKE
#define DECK_ENABLE_EARTHQUAKE 1
#endif
#ifndef DECK_ENABLE_WILDFIRE
#define DECK_ENABLE_WILDFIRE 1
#endif
#ifndef DECK_ENABLE_PRINTER
#define DECK_ENABLE_PRINTER 1
#endif
#ifndef DECK_ENABLE_SPACE
#define DECK_ENABLE_SPACE 1
#endif
#if !(DECK_ENABLE_FLIGHT || DECK_ENABLE_GAS || DECK_ENABLE_EARTHQUAKE || DECK_ENABLE_WILDFIRE || DECK_ENABLE_PRINTER || DECK_ENABLE_SPACE)
#error Select at least one Widget Deck widget.
#endif
#if DECK_ENABLE_FLIGHT
#include "flight/flight_widget.h"
#endif
#if DECK_ENABLE_GAS
#include "gas/gas_widget.h"
#endif
#if DECK_ENABLE_EARTHQUAKE
#include "earthquake/earthquake_widget.h"
#endif
#if DECK_ENABLE_WILDFIRE
#include "wildfire/wildfire_widget.h"
#endif
#if DECK_ENABLE_PRINTER
#include "printer/printer_widget.h"
#endif
#if DECK_ENABLE_SPACE
#include "space/space_widget.h"
#endif
namespace deck {
// Add a widget header, instance and pointer here. Array order is swipe order.
#if DECK_ENABLE_FLIGHT
static FlightWidget flight;
#endif
#if DECK_ENABLE_GAS
static GasWidget gasPrice;
#endif
#if DECK_ENABLE_EARTHQUAKE
static EarthquakeWidget earthquakes;
#endif
#if DECK_ENABLE_WILDFIRE
static WildfireWidget wildfires;
#endif
#if DECK_ENABLE_PRINTER
static PrinterWidget printerStatus;
#endif
#if DECK_ENABLE_SPACE
static SpaceWidget spaceWeather;
#endif
static Widget *registry[]={
#if DECK_ENABLE_FLIGHT
  &flight,
#endif
#if DECK_ENABLE_GAS
  &gasPrice,
#endif
#if DECK_ENABLE_EARTHQUAKE
  &earthquakes,
#endif
#if DECK_ENABLE_WILDFIRE
  &wildfires,
#endif
#if DECK_ENABLE_PRINTER
  &printerStatus,
#endif
#if DECK_ENABLE_SPACE
  &spaceWeather,
#endif
};
static Deck app(registry,sizeof(registry)/sizeof(registry[0]));
}
