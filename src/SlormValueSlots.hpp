#pragma once
#include <charconv>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

namespace Slorm {
inline std::vector<std::string> SplitSlots(const std::string& text) {
    std::vector<std::string> parts; size_t start=0;
    for (;;) {
        auto end=text.find('|',start); parts.push_back(text.substr(start,end==std::string::npos?end:end-start));
        if(parts.size()>16)throw std::runtime_error("At most 16 stat result slots are supported");
        if(end==std::string::npos)break;start=end+1;
    }
    return parts;
}
inline const std::string& Slot(const std::vector<std::string>& parts,int index) {
    if(index<0||static_cast<size_t>(index)>=parts.size())throw std::runtime_error("Requested stat result slot is absent");return parts[index];
}
inline double NumericText(const std::string& text) {
    const char* first=text.data();const char* last=first+text.size();
    if(first!=last&&*first=='+') {++first;if(first!=last&&(*first=='+'||*first=='-'))throw std::runtime_error("Expected one numeric sign");}
    double value{};auto parsed=std::from_chars(first,last,value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=last||!std::isfinite(value))throw std::runtime_error("Expected an exact finite numeric value");return value;
}
inline std::string NumericString(double value) {
    char text[80];auto result=std::to_chars(text,text+sizeof(text),value);
    if(result.ec!=std::errc{}||!std::isfinite(value))throw std::runtime_error("Unable to format stat value");return std::string(text,result.ptr);
}
inline std::string ReplaceSlot(const std::string& text,int index,const std::string& value) {
    auto parts=SplitSlots(text);Slot(parts,index);parts[index]=value;
    std::string result;
    for(size_t i=0;i<parts.size();++i){if(i)result+='|';result+=parts[i];}
    return result;
}
}
