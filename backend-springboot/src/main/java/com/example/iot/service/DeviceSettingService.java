package com.example.iot.service;

import com.example.iot.dto.DeviceSettingRequest;
import com.example.iot.dto.DeviceSettingResponse;
import com.example.iot.entity.DeviceSetting;
import com.example.iot.repository.DeviceRepository;
import com.example.iot.repository.DeviceSettingRepository;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.server.ResponseStatusException;
import java.util.LinkedHashMap;
import java.util.UUID;

@Service
@RequiredArgsConstructor
public class DeviceSettingService {
    private static final double DEFAULT_TEMPERATURE_THRESHOLD = 32.0;
    private static final double DEFAULT_SOIL_MOISTURE_MIN = 30.0;
    private static final double DEFAULT_SOIL_MOISTURE_MAX = 80.0;
    private final DeviceSettingRepository deviceSettingRepository;
    private final DeviceRepository deviceRepository;
    private final CommandService commandService;

    @Transactional
    public DeviceSettingResponse getSettings(String deviceId) {
        requireDevice(deviceId);
        return toResponse(deviceSettingRepository.findByDeviceId(deviceId).orElseGet(() -> deviceSettingRepository.save(defaultSettings(deviceId))));
    }

    @Transactional
    public DeviceSettingResponse updateSettings(String deviceId, DeviceSettingRequest request) {
        requireDevice(deviceId);
        if (request.getSoilMoistureMin() > request.getSoilMoistureMax()) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Soil moisture minimum must not exceed maximum.");
        }
        DeviceSetting settings = deviceSettingRepository.findByDeviceId(deviceId).orElseGet(() -> defaultSettings(deviceId));
        settings.setTemperatureThreshold(request.getTemperatureThreshold());
        settings.setSoilMoistureMin(request.getSoilMoistureMin());
        settings.setSoilMoistureMax(request.getSoilMoistureMax());
        deviceSettingRepository.save(settings);
        LinkedHashMap<String, Object> commandData = new LinkedHashMap<>();
        commandData.put("temperatureThreshold", settings.getTemperatureThreshold());
        commandData.put("soilMoistureMin", settings.getSoilMoistureMin());
        commandData.put("soilMoistureMax", settings.getSoilMoistureMax());
        commandService.sendCommand(deviceId, "SET_THRESHOLD", commandData);
        return toResponse(settings);
    }

    private void requireDevice(String deviceId) {
        if (!deviceRepository.existsByDeviceId(deviceId)) throw new ResponseStatusException(HttpStatus.NOT_FOUND, "Device not found: " + deviceId);
    }
    private DeviceSetting defaultSettings(String deviceId) {
        DeviceSetting settings = new DeviceSetting();
        settings.setId(UUID.randomUUID()); settings.setDeviceId(deviceId);
        settings.setTemperatureThreshold(DEFAULT_TEMPERATURE_THRESHOLD);
        settings.setSoilMoistureMin(DEFAULT_SOIL_MOISTURE_MIN); settings.setSoilMoistureMax(DEFAULT_SOIL_MOISTURE_MAX);
        return settings;
    }
    private DeviceSettingResponse toResponse(DeviceSetting settings) {
        return new DeviceSettingResponse(settings.getDeviceId(), settings.getTemperatureThreshold(), settings.getSoilMoistureMin(), settings.getSoilMoistureMax());
    }
}
