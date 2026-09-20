package com.example.iot.repository;

import com.example.iot.entity.DeviceSetting;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.Optional;
import java.util.UUID;

public interface DeviceSettingRepository extends JpaRepository<DeviceSetting, UUID> {
    Optional<DeviceSetting> findByDeviceId(String deviceId);
}
