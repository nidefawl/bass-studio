#pragma once
#include "assert_dbg.h"
#include <clap/ext/params.h>
#include <ostream>
#include <unordered_map>
#include <clap/clap.h>

class clapplugin;
class PluginParam {
public:
   PluginParam(clapplugin &pluginHost, const clap_param_info &info, double value);

   double value() const { return _value; }
   void setValue(double v);

   double modulation() const { return _modulation; }
   void setModulation(double v);

   double modulatedValue() const {
      return std::min(_info.max_value, std::max(_info.min_value, _value + _modulation));
   }

   bool isValueValid(const double v) const;

   void printShortInfo(std::ostream &os) const;
   void printInfo(std::ostream &os) const;

   void setInfo(const clap_param_info &info) noexcept { _info = info; }
   bool isInfoEqualTo(const clap_param_info &info) const;
   bool isInfoCriticallyDifferentTo(const clap_param_info &info) const;
   clap_param_info &info() noexcept { return _info; }
   bool isReadOnly() const noexcept { return _info.flags & CLAP_PARAM_IS_READONLY; }
   bool isModulatable() const noexcept { return !isReadOnly() && _info.flags & CLAP_PARAM_IS_MODULATABLE; }
   bool isAutomatable() const noexcept { return !isReadOnly() && _info.flags & CLAP_PARAM_IS_AUTOMATABLE; }
   const clap_param_info &info() const noexcept { return _info; }

   bool isBeingAdjusted() const noexcept { return _isBeingAdjusted; }
   void setIsAdjusting(bool isAdjusting) {
      _isBeingAdjusted = isAdjusting;
   }

public: //previously signals
   void infoChanged() { /* TODO */ }
   void valueChanged() { /* TODO */ }
   void modulatedValueChanged() { /* TODO */ }

private:
   bool _isBeingAdjusted = false;
   clap_param_info _info;
   double _value = 0;
   double _modulation = 0;
   std::unordered_map<int64_t, std::string> _enumEntries;
};