from __future__ import annotations

import json
import time
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np


OUTPUT_DIR = Path(r"C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\artifacts")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUTPUT_PATH = OUTPUT_DIR / "usb_camera_snapshot.jpg"
METADATA_PATH = OUTPUT_DIR / "usb_camera_snapshot.metadata.json"

FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
WARMUP_MIN_FRAMES = 24
WARMUP_MIN_SECONDS = 1.8
WARMUP_MAX_SECONDS = 5.0
SELECTION_FRAME_COUNT = 8
SELECTION_MAX_SECONDS = 2.5
READ_TIMEOUT_SECONDS = 6.0
JPEG_QUALITY = 95

BACKENDS = [cv2.CAP_DSHOW, cv2.CAP_MSMF, cv2.CAP_ANY]
BACKEND_NAMES = {
    int(cv2.CAP_ANY): "ANY",
    int(cv2.CAP_DSHOW): "DSHOW",
    int(cv2.CAP_MSMF): "MSMF",
}

AUTO_SETTINGS = (
    (cv2.CAP_PROP_AUTO_EXPOSURE, 0.75),
    (cv2.CAP_PROP_AUTO_EXPOSURE, 1.0),
    (cv2.CAP_PROP_AUTO_WB, 1.0),
    (cv2.CAP_PROP_AUTOFOCUS, 1.0),
)


def round_float(value: float, digits: int = 2) -> float:
    return round(float(value), digits)


def backend_name(backend: int) -> str:
    return BACKEND_NAMES.get(int(backend), f"BACKEND_{int(backend)}")


def set_capture_properties(cap: cv2.VideoCapture) -> None:
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS, 30)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    for prop_id, value in AUTO_SETTINGS:
        try:
            cap.set(prop_id, value)
        except Exception:
            continue


def read_valid_frame(cap: cv2.VideoCapture, timeout_seconds: float = READ_TIMEOUT_SECONDS) -> np.ndarray | None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        ok, frame = cap.read()
        if ok and frame is not None and frame.size > 0:
            return frame
        time.sleep(0.05)
    return None


def compute_frame_metrics(frame: np.ndarray, previous_gray: np.ndarray | None) -> tuple[dict[str, float | str], np.ndarray]:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    channel_means = np.mean(frame, axis=(0, 1))
    brightness = float(np.mean(gray))
    contrast = float(np.std(gray))
    sharpness = float(cv2.Laplacian(gray, cv2.CV_64F).var())
    low_clip_pct = float(np.mean(gray <= 8) * 100.0)
    high_clip_pct = float(np.mean(gray >= 247) * 100.0)
    saturation_pct = float(np.mean(hsv[:, :, 1] >= 245) * 100.0)
    motion_mean_abs_diff = 0.0
    if previous_gray is not None:
        motion_mean_abs_diff = float(np.mean(cv2.absdiff(gray, previous_gray)))

    channel_mean = float(np.mean(channel_means))
    channel_spread = float(np.max(channel_means) - np.min(channel_means))
    wb_delta_pct = (channel_spread / max(channel_mean, 1.0)) * 100.0
    temperature_bias_pct = ((float(channel_means[2]) - float(channel_means[0])) / max(channel_mean, 1.0)) * 100.0

    exposure_score = max(0.0, 1.0 - abs(brightness - 132.0) / 90.0)
    clipping_penalty = min(1.0, (low_clip_pct + high_clip_pct) / 18.0)
    exposure_score *= 1.0 - (0.65 * clipping_penalty)
    white_balance_score = max(0.0, 1.0 - wb_delta_pct / 38.0)
    sharpness_score = sharpness / (sharpness + 140.0)
    stability_score = max(0.0, 1.0 - motion_mean_abs_diff / 22.0)
    contrast_score = min(1.0, contrast / 52.0)
    score = (
        exposure_score * 0.38
        + white_balance_score * 0.20
        + sharpness_score * 0.27
        + stability_score * 0.10
        + contrast_score * 0.05
    )

    if temperature_bias_pct > 8.0:
        white_balance_cast = "warm"
    elif temperature_bias_pct < -8.0:
        white_balance_cast = "cool"
    else:
        white_balance_cast = "neutral"

    metrics = {
        "mean_brightness": round_float(brightness),
        "contrast_stddev": round_float(contrast),
        "sharpness_laplacian_var": round_float(sharpness),
        "low_clip_pct": round_float(low_clip_pct),
        "high_clip_pct": round_float(high_clip_pct),
        "saturated_color_pct": round_float(saturation_pct),
        "motion_mean_abs_diff": round_float(motion_mean_abs_diff),
        "channel_mean_b": round_float(channel_means[0]),
        "channel_mean_g": round_float(channel_means[1]),
        "channel_mean_r": round_float(channel_means[2]),
        "white_balance_delta_pct": round_float(wb_delta_pct),
        "temperature_bias_pct": round_float(temperature_bias_pct),
        "white_balance_cast": white_balance_cast,
        "exposure_score": round_float(exposure_score, 3),
        "white_balance_score": round_float(white_balance_score, 3),
        "sharpness_score": round_float(sharpness_score, 3),
        "stability_score": round_float(stability_score, 3),
        "contrast_score": round_float(contrast_score, 3),
        "selection_score": round_float(score, 3),
    }
    return metrics, gray


def warmup_camera(cap: cv2.VideoCapture) -> tuple[np.ndarray | None, dict[str, float | int | bool]]:
    start = time.monotonic()
    last_frame = None
    last_gray = None
    stability_history: list[dict[str, float | str]] = []
    warmup_frames = 0
    stabilized = False

    while True:
        frame = read_valid_frame(cap, timeout_seconds=0.75)
        elapsed = time.monotonic() - start
        if frame is None:
            if elapsed >= WARMUP_MAX_SECONDS:
                break
            continue

        metrics, last_gray = compute_frame_metrics(frame, last_gray)
        last_frame = frame
        warmup_frames += 1
        stability_history.append(metrics)
        stability_history = stability_history[-6:]

        if len(stability_history) >= 4:
            brightness_std = float(np.std([item["mean_brightness"] for item in stability_history]))
            wb_std = float(np.std([item["white_balance_delta_pct"] for item in stability_history]))
            stabilized = brightness_std <= 5.0 and wb_std <= 4.0
        else:
            brightness_std = 999.0
            wb_std = 999.0

        if (
            warmup_frames >= WARMUP_MIN_FRAMES
            and elapsed >= WARMUP_MIN_SECONDS
            and stabilized
        ):
            break
        if elapsed >= WARMUP_MAX_SECONDS:
            break

    warmup_summary = {
        "frames": warmup_frames,
        "seconds": round_float(time.monotonic() - start, 3),
        "stabilized": stabilized,
        "brightness_stddev": round_float(brightness_std),
        "white_balance_stddev": round_float(wb_std),
    }
    return last_frame, warmup_summary


def select_best_frame(cap: cv2.VideoCapture, initial_frame: np.ndarray | None) -> tuple[np.ndarray | None, dict[str, object]]:
    start = time.monotonic()
    candidates: list[dict[str, object]] = []
    best_frame = initial_frame
    best_metrics = None
    previous_gray = None

    if initial_frame is not None:
        initial_metrics, previous_gray = compute_frame_metrics(initial_frame, None)
        candidates.append({"frame_index": 0, "metrics": initial_metrics})
        best_frame = initial_frame
        best_metrics = initial_metrics

    while len(candidates) < SELECTION_FRAME_COUNT and (time.monotonic() - start) < SELECTION_MAX_SECONDS:
        frame = read_valid_frame(cap, timeout_seconds=0.6)
        if frame is None:
            continue

        metrics, previous_gray = compute_frame_metrics(frame, previous_gray)
        frame_index = len(candidates)
        candidates.append({"frame_index": frame_index, "metrics": metrics})
        if best_metrics is None or metrics["selection_score"] > best_metrics["selection_score"]:
            best_frame = frame
            best_metrics = metrics

    selection_summary = {
        "candidate_count": len(candidates),
        "selected_frame_index": next(
            (
                item["frame_index"]
                for item in candidates
                if item["metrics"] == best_metrics
            ),
            0,
        ),
        "selection_seconds": round_float(time.monotonic() - start, 3),
        "candidates": [
            {
                "frame_index": item["frame_index"],
                **item["metrics"],
            }
            for item in candidates
        ],
        "best_metrics": best_metrics,
    }
    return best_frame, selection_summary


def try_camera(index: int) -> tuple[np.ndarray | None, dict[str, object] | None]:
    errors: list[str] = []
    for backend in BACKENDS:
        cap = cv2.VideoCapture(index, backend)
        if not cap.isOpened():
            cap.release()
            errors.append(f"{backend_name(backend)} open_failed")
            continue

        try:
            set_capture_properties(cap)
            warmup_frame, warmup_summary = warmup_camera(cap)
            frame, selection_summary = select_best_frame(cap, warmup_frame)
            if frame is None:
                errors.append(f"{backend_name(backend)} no_valid_frame")
                continue

            return frame, {
                "camera_index": index,
                "backend": int(backend),
                "backend_name": backend_name(backend),
                "warmup": warmup_summary,
                "selection": selection_summary,
            }
        finally:
            cap.release()

    return None, {"camera_index": index, "errors": errors}


def build_notes(metrics: dict[str, float | str]) -> list[str]:
    notes: list[str] = []
    if metrics["mean_brightness"] < 90:
        notes.append("Image is dark; GPT should account for underexposure.")
    elif metrics["mean_brightness"] > 175:
        notes.append("Image is bright; GPT should watch for highlight washout.")

    if metrics["white_balance_cast"] != "neutral":
        notes.append(f"White balance trends {metrics['white_balance_cast']}.")

    if metrics["sharpness_laplacian_var"] < 80:
        notes.append("Frame sharpness is low; fine text may be unreliable.")

    if metrics["high_clip_pct"] > 2.5 or metrics["low_clip_pct"] > 10.0:
        notes.append("Clipping is present in shadows or highlights.")

    if metrics["motion_mean_abs_diff"] > 8.0:
        notes.append("Scene motion was detected during frame selection.")

    if not notes:
        notes.append("Exposure, white balance, and sharpness are within the expected range.")
    return notes


def write_metadata(metadata: dict[str, object]) -> None:
    METADATA_PATH.write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def main() -> None:
    attempts: list[dict[str, object]] = []
    for index in range(4):
        frame, capture = try_camera(index)
        if capture is not None:
            attempts.append(capture)
        if frame is None or capture is None:
            continue

        params = [cv2.IMWRITE_JPEG_QUALITY, JPEG_QUALITY]
        if not cv2.imwrite(str(OUTPUT_PATH), frame, params):
            print(json.dumps({"ok": False, "error": "Failed to write USB camera snapshot"}, ensure_ascii=False))
            raise SystemExit(1)

        image_metrics, _ = compute_frame_metrics(frame, None)
        notes = build_notes(image_metrics)
        captured_at = datetime.now().astimezone().isoformat(timespec="seconds")
        metadata = {
            "ok": True,
            "camera_index": capture["camera_index"],
            "backend": capture["backend"],
            "backend_name": capture["backend_name"],
            "captured_at": captured_at,
            "width": int(frame.shape[1]),
            "height": int(frame.shape[0]),
            "mean_brightness": image_metrics["mean_brightness"],
            "snapshot_path": str(OUTPUT_PATH),
            "metadata_path": str(METADATA_PATH),
            "warmup": capture["warmup"],
            "selection": {
                "candidate_count": capture["selection"]["candidate_count"],
                "selected_frame_index": capture["selection"]["selected_frame_index"],
                "selection_seconds": capture["selection"]["selection_seconds"],
                "best_metrics": capture["selection"]["best_metrics"],
            },
            "image_metrics": image_metrics,
            "notes": notes,
            "gpt_handoff": {
                "artifact_type": "image_with_metadata",
                "image_path": str(OUTPUT_PATH),
                "metadata_path": str(METADATA_PATH),
                "prompt": (
                    "Analyze the attached USB camera snapshot of the ESP32 sensor hub test setup. "
                    "Use the metadata JSON for capture quality signals. Check framing, glare, blur, "
                    "display readability, device presence, cable visibility, and any obvious test setup issues."
                ),
                "checklist": [
                    "Is the device or dashboard visible and reasonably framed?",
                    "Is the image too dark, too bright, blurry, or color shifted?",
                    "Are there obvious occlusions, reflections, or missing hardware cues?",
                ],
            },
            "artifact_bundle": {
                "image_path": str(OUTPUT_PATH),
                "metadata_path": str(METADATA_PATH),
            },
        }
        write_metadata(
            {
                **metadata,
                "selection": capture["selection"],
                "attempts": attempts,
            }
        )
        print(json.dumps(metadata, ensure_ascii=False))
        return

    error_result = {
        "ok": False,
        "error": "No USB camera frame captured",
        "attempts": attempts,
    }
    print(json.dumps(error_result, ensure_ascii=False))
    raise SystemExit(1)


if __name__ == "__main__":
    main()
