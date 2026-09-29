#pragma once
#include "sky_data.h"
#include "sky_airlines.h"

namespace sky {
enum class Role : uint8_t { Standard, Military, Police, Rescue, Fire };

// ADS-B flight identification is at most eight characters. Require an ICAO
// operator followed by a flight number, never an arbitrary substring or tail.
inline bool operatorIs(const Aircraft &a,const char *code) {
  size_t n=strlen(a.flight);
  if(n<4||n>8||strncmp(a.flight,code,3)||a.flight[3]<'0'||a.flight[3]>'9')return false;
  for(size_t i=4;i<n;i++)if(!((a.flight[i]>='0'&&a.flight[i]<='9')||(a.flight[i]>='A'&&a.flight[i]<='Z')))return false;
  return true;
}
inline Role role(const Aircraft &a) {
  // Only bit 0 means military; interesting, privacy and blocked flags do not.
  if(a.dbFlags&1u)return Role::Military;
  struct Operator {const char *code;Role role;};
  // FAA operator designators, verified September 2026. These identify an
  // operator's service, not proof of the aircraft's current mission.
  static const Operator services[]={
    {"CFR",Role::Fire},{"CUL",Role::Fire},
    {"REH",Role::Rescue},{"CMD",Role::Rescue},{"RGA",Role::Rescue},
    {"SAZ",Role::Rescue},{"NVT",Role::Rescue},{"DOK",Role::Rescue},
    {"TRP",Role::Police},{"GRY",Role::Police},{"KAN",Role::Police},
    {"GRM",Role::Police},{"UKP",Role::Police},{"SST",Role::Police},
    {"FPL",Role::Police},{"POF",Role::Police},{"HEP",Role::Police},
    {"UAP",Role::Police}
  };
  for(const auto &service:services)if(operatorIs(a,service.code))return service.role;
  if(a.medicalPriority)return Role::Rescue;
  return Role::Standard;
}
inline const char *roleName(Role value) {
  switch(value){case Role::Military:return "MILITARY";case Role::Police:return "POLICE";
    case Role::Rescue:return "RESCUE";case Role::Fire:return "FIRE";default:return "";}
}
inline const Airline *airline(const Aircraft &a) {
  if(role(a)!=Role::Standard)return nullptr;
  for(const auto &brand:Airlines)if(operatorIs(a,brand.code))return &brand;
  return nullptr;
}
}
