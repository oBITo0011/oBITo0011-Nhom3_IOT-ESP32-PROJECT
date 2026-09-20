package com.example.iot.entity;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import lombok.Data;
import java.util.UUID;

@Data
@Entity
@Table(name = "device_settings")
public class DeviceSetting {
    @Id private UUID id;
    @Column(name = "device_id", nullable = false, unique = true) private String deviceId;
    @Column(name = "temperature_threshold", nullable = false) private Double temperatureThreshold;
    @Column(name = "soil_moisture_min", nullable = false) private Double soilMoistureMin;
    @Column(name = "soil_moisture_max", nullable = false) private Double soilMoistureMax;
}
