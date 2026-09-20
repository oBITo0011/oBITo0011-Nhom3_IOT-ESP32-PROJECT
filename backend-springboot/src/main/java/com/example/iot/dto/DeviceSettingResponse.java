package com.example.iot.dto;

import lombok.AllArgsConstructor;
import lombok.Data;

@Data
@AllArgsConstructor
public class DeviceSettingResponse {
    private String deviceId;
    private Double temperatureThreshold;
    private Double soilMoistureMin;
    private Double soilMoistureMax;
}
