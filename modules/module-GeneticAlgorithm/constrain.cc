#pragma once
#include <string>
#include <sstream>
#include <vector>

class HFElemConstraints {
private:
    double m_dAmplitudeLow;  // Lower bound
    double m_dAmplitudeUpp;  // Upper bound
    double m_maxRMSError;    // Maximum allowed RMS error
    
public:
    HFElemConstraints(double low = 0.1, double upp = 10.0, double maxError = 0.1)
        : m_dAmplitudeLow(low), m_dAmplitudeUpp(upp), m_maxRMSError(maxError) {}

    // Check constraints for single amplitude
    bool checkConstraints(double amplitude, double rmsError, std::string& outMsg) const {
        std::ostringstream oss;
        bool valid = true;

        // Check amplitude bounds
        if(amplitude < m_dAmplitudeLow) {
            valid = false;
            oss << "Amplitude " << amplitude << " below minimum " << m_dAmplitudeLow;
        }
        
        if(amplitude > m_dAmplitudeUpp) {
            if(!valid) oss << "; ";
            valid = false;
            oss << "Amplitude " << amplitude << " above maximum " << m_dAmplitudeUpp;
        }

        // Check RMS error constraint
        if(rmsError > m_maxRMSError) {
            if(!valid) oss << "; ";
            valid = false;
            oss << "RMS error " << rmsError << " exceeds maximum " << m_maxRMSError;
        }

        outMsg = oss.str();
        return valid;
    }

    // Check constraints for entire population
    std::vector<bool> validatePopulation(
        const std::vector<double>& amplitudes,
        const std::vector<double>& rmsErrors,
        std::vector<std::string>& messages) const {
        
        std::vector<bool> valid;
        messages.resize(amplitudes.size());
        valid.reserve(amplitudes.size());
        
        for(size_t i = 0; i < amplitudes.size(); i++) {
            valid.push_back(checkConstraints(amplitudes[i], rmsErrors[i], messages[i]));
        }
        return valid;
    }
};