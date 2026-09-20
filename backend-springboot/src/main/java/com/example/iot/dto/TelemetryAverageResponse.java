package com.example.iot.dto;

import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class TelemetryAverageResponse {
    private String period;
    private Double averageTemperature;
    private Double averageHumidity;
    private Double averageIlluminance;
    private Integer sampleCount;
}
