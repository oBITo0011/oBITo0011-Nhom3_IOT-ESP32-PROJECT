package com.example.iot.dto;

import jakarta.validation.constraints.DecimalMax;
import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

@Data
public class DeviceSettingRequest {
    @NotNull @DecimalMin(value = "0.0", inclusive = false) @DecimalMax("100.0")
    private Double temperatureThreshold;
    @NotNull @DecimalMin("0.0") @DecimalMax("100.0")
    private Double soilMoistureMin;
    @NotNull @DecimalMin("0.0") @DecimalMax("100.0")
    private Double soilMoistureMax;
}
