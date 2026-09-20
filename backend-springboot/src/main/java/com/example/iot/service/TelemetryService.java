package com.example.iot.service;

import com.example.iot.dto.TelemetryAverageResponse;
import com.example.iot.dto.TelemetryPayload;
import com.example.iot.entity.Device;
import com.example.iot.entity.Telemetry;
import com.example.iot.repository.DeviceRepository;
import com.example.iot.repository.TelemetryRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDate;
import java.time.ZonedDateTime;
import java.time.temporal.ChronoUnit;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.stream.Collectors;

@Slf4j
@Service
@RequiredArgsConstructor
public class TelemetryService {

    private final TelemetryRepository telemetryRepository;
    private final DeviceRepository deviceRepository;

    // ============================================================
    // NHẬN VÀ LƯU TELEMETRY
    // ============================================================

    @Transactional
    public void processTelemetry(TelemetryPayload payload) {

        if (payload.getDeviceId() == null) {
            return;
        }

        Optional<Device> deviceOpt =
                deviceRepository.findByDeviceId(payload.getDeviceId());

        if (deviceOpt.isPresent()) {

            Device device = deviceOpt.get();

            /* lastSeenAt is the backend receive time, not an untrusted device clock. */
            device.setLastSeenAt(ZonedDateTime.now());

            if (payload.getLed() != null) {
                device.setLedState(payload.getLed());
            }

            if ("OFFLINE".equals(device.getStatus())) {
                device.setStatus("ONLINE");
            }

            device.setUpdatedAt(ZonedDateTime.now());

            deviceRepository.save(device);

            Telemetry telemetry = new Telemetry();

            telemetry.setId(java.util.UUID.randomUUID());
            telemetry.setDeviceId(payload.getDeviceId());
            telemetry.setTemperature(payload.getTemperature());
            telemetry.setHumidity(payload.getHumidity());
            telemetry.setIlluminance(payload.getIlluminance());
            telemetry.setSoilMoisture(payload.getSoilMoisture());
            telemetry.setLedState(payload.getLed());

            telemetry.setRecordedAt(
                    payload.getTimestamp() != null
                            ? payload.getTimestamp()
                            : ZonedDateTime.now()
            );

            telemetry.setReceivedAt(ZonedDateTime.now());

            telemetryRepository.save(telemetry);

            log.info(
                    "Saved telemetry for device: {}",
                    payload.getDeviceId()
            );

        } else {

            log.warn(
                    "Telemetry received for unknown device: {}",
                    payload.getDeviceId()
            );
        }
    }

    // ============================================================
    // LẤY TELEMETRY MỚI NHẤT
    // ============================================================

    public Telemetry getLatestTelemetry(String deviceId) {

        return telemetryRepository
                .findFirstByDeviceIdOrderByRecordedAtDesc(deviceId)
                .orElse(null);
    }

    // ============================================================
    // LẤY LỊCH SỬ TELEMETRY
    // ============================================================

    public Page<Telemetry> getTelemetryHistory(
            String deviceId,
            ZonedDateTime from,
            ZonedDateTime to,
            Pageable pageable) {

        if (from != null && to != null) {

            return telemetryRepository
                    .findByDeviceIdAndRecordedAtBetweenOrderByRecordedAtDesc(
                            deviceId,
                            from,
                            to,
                            pageable
                    );
        }

        return telemetryRepository
                .findByDeviceIdOrderByRecordedAtDesc(
                        deviceId,
                        pageable
                );
    }

    // ============================================================
    // TRUNG BÌNH THEO GIỜ
    // ============================================================

    public List<TelemetryAverageResponse> getHourlyAverages(
            String deviceId,
            ZonedDateTime from,
            ZonedDateTime to) {

        List<Telemetry> telemetryList =
                telemetryRepository
                        .findByDeviceIdAndRecordedAtBetweenOrderByRecordedAtAsc(
                                deviceId,
                                from,
                                to
                        );

        Map<ZonedDateTime, List<Telemetry>> groupedByHour =
                telemetryList.stream()
                        .filter(t -> t.getRecordedAt() != null)
                        .collect(Collectors.groupingBy(
                                t -> t.getRecordedAt()
                                        .truncatedTo(ChronoUnit.HOURS),
                                LinkedHashMap::new,
                                Collectors.toList()
                        ));

        List<TelemetryAverageResponse> result = new ArrayList<>();

        groupedByHour.entrySet()
                .stream()
                .sorted(Map.Entry.comparingByKey())
                .forEach(entry -> {

                    ZonedDateTime hour = entry.getKey();

                    List<Telemetry> items = entry.getValue();

                    result.add(
                            calculateAverage(
                                    hour.toString(),
                                    items
                            )
                    );
                });

        return result;
    }

    // ============================================================
    // TRUNG BÌNH THEO NGÀY
    // ============================================================

    public List<TelemetryAverageResponse> getDailyAverages(
            String deviceId,
            ZonedDateTime from,
            ZonedDateTime to) {

        List<Telemetry> telemetryList =
                telemetryRepository
                        .findByDeviceIdAndRecordedAtBetweenOrderByRecordedAtAsc(
                                deviceId,
                                from,
                                to
                        );

        Map<LocalDate, List<Telemetry>> groupedByDay =
                telemetryList.stream()
                        .filter(t -> t.getRecordedAt() != null)
                        .collect(Collectors.groupingBy(
                                t -> t.getRecordedAt().toLocalDate(),
                                LinkedHashMap::new,
                                Collectors.toList()
                        ));

        List<TelemetryAverageResponse> result = new ArrayList<>();

        groupedByDay.entrySet()
                .stream()
                .sorted(Map.Entry.comparingByKey())
                .forEach(entry -> {

                    LocalDate day = entry.getKey();

                    List<Telemetry> items = entry.getValue();

                    result.add(
                            calculateAverage(
                                    day.toString(),
                                    items
                            )
                    );
                });

        return result;
    }

    // ============================================================
    // TRUNG BÌNH THEO THÁNG
    // ============================================================

    public List<TelemetryAverageResponse> getMonthlyAverages(
            String deviceId,
            ZonedDateTime from,
            ZonedDateTime to) {

        List<Telemetry> telemetryList =
                telemetryRepository
                        .findByDeviceIdAndRecordedAtBetweenOrderByRecordedAtAsc(
                                deviceId,
                                from,
                                to
                        );

        Map<String, List<Telemetry>> groupedByMonth =
                telemetryList.stream()
                        .filter(t -> t.getRecordedAt() != null)
                        .collect(Collectors.groupingBy(
                                t -> String.format(
                                        "%04d-%02d",
                                        t.getRecordedAt().getYear(),
                                        t.getRecordedAt().getMonthValue()
                                ),
                                LinkedHashMap::new,
                                Collectors.toList()
                        ));

        List<TelemetryAverageResponse> result = new ArrayList<>();

        groupedByMonth.entrySet()
                .stream()
                .sorted(Map.Entry.comparingByKey())
                .forEach(entry -> {

                    result.add(
                            calculateAverage(
                                    entry.getKey(),
                                    entry.getValue()
                            )
                    );
                });

        return result;
    }

    // ============================================================
    // TRUNG BÌNH THEO NĂM
    // ============================================================

    public List<TelemetryAverageResponse> getYearlyAverages(
            String deviceId,
            ZonedDateTime from,
            ZonedDateTime to) {

        List<Telemetry> telemetryList =
                telemetryRepository
                        .findByDeviceIdAndRecordedAtBetweenOrderByRecordedAtAsc(
                                deviceId,
                                from,
                                to
                        );

        Map<Integer, List<Telemetry>> groupedByYear =
                telemetryList.stream()
                        .filter(t -> t.getRecordedAt() != null)
                        .collect(Collectors.groupingBy(
                                t -> t.getRecordedAt().getYear(),
                                LinkedHashMap::new,
                                Collectors.toList()
                        ));

        List<TelemetryAverageResponse> result = new ArrayList<>();

        groupedByYear.entrySet()
                .stream()
                .sorted(Map.Entry.comparingByKey())
                .forEach(entry -> {

                    result.add(
                            calculateAverage(
                                    String.valueOf(entry.getKey()),
                                    entry.getValue()
                            )
                    );
                });

        return result;
    }

    // ============================================================
    // HÀM TÍNH TRUNG BÌNH
    // ============================================================

    private TelemetryAverageResponse calculateAverage(
            String period,
            List<Telemetry> items) {

        double temperatureSum = 0.0;
        double humiditySum = 0.0;
        double illuminanceSum = 0.0;

        int temperatureCount = 0;
        int humidityCount = 0;
        int illuminanceCount = 0;

        for (Telemetry telemetry : items) {

            // Nhiệt độ
            if (telemetry.getTemperature() != null) {
                temperatureSum += telemetry.getTemperature();
                temperatureCount++;
            }

            // Độ ẩm
            if (telemetry.getHumidity() != null) {
                humiditySum += telemetry.getHumidity();
                humidityCount++;
            }

            // Ánh sáng
            if (telemetry.getIlluminance() != null) {
                illuminanceSum += telemetry.getIlluminance();
                illuminanceCount++;
            }
        }

        Double averageTemperature =
                temperatureCount > 0
                        ? round(temperatureSum / temperatureCount)
                        : null;

        Double averageHumidity =
                humidityCount > 0
                        ? round(humiditySum / humidityCount)
                        : null;

        Double averageIlluminance =
                illuminanceCount > 0
                        ? round(illuminanceSum / illuminanceCount)
                        : null;

        return new TelemetryAverageResponse(
                period,
                averageTemperature,
                averageHumidity,
                averageIlluminance,
                items.size()
        );
    }

    // ============================================================
    // LÀM TRÒN 2 CHỮ SỐ THẬP PHÂN
    // ============================================================

    private Double round(Double value) {

        if (value == null) {
            return null;
        }

        return Math.round(value * 100.0) / 100.0;
    }
}
