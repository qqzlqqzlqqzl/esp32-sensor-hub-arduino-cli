from __future__ import annotations

import json
import math
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

BRIGHT_SCENE_TRIGGER_BRIGHTNESS = 175.0
BRIGHT_SCENE_TRIGGER_HIGHLIGHT_CLIP = 8.0
BRIGHT_SCENE_GOAL_HIGHLIGHT_CLIP = 5.0
BRIGHT_SCENE_MIN_BRIGHTNESS = 72.0
BRIGHT_SCENE_MAX_LOW_CLIP = 26.0
BRIGHT_SCENE_MIN_SHARPNESS = 55.0
FALLBACK_WARMUP_MIN_FRAMES = 10
FALLBACK_WARMUP_MIN_SECONDS = 0.8
FALLBACK_WARMUP_MAX_SECONDS = 2.0
FALLBACK_SELECTION_FRAME_COUNT = 5
FALLBACK_SELECTION_MAX_SECONDS = 1.3
FALLBACK_SETTLE_SECONDS = 0.18
MAX_EXPOSURE_CANDIDATES = 8

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

MANUAL_AUTO_EXPOSURE_VALUES = (0.25, 0.0, 1.0)
COMMON_NEGATIVE_EXPOSURES = (-4.0, -5.0, -6.0, -7.0, -8.0, -9.0, -10.0, -11.0)
COMMON_POSITIVE_EXPOSURES = (0.00390625, 0.0078125, 0.015625, 0.03125, 0.0625, 0.125)


def configure_opencv_logging() -> None:
    try:
        if hasattr(cv2, "utils") and hasattr(cv2.utils, "logging"):
            logging_api = cv2.utils.logging
            silent_level = getattr(logging_api, "LOG_LEVEL_SILENT", None)
            error_level = getattr(logging_api, "LOG_LEVEL_ERROR", None)
            if silent_level is not None:
                logging_api.setLogLevel(silent_level)
                return
            if error_level is not None:
                logging_api.setLogLevel(error_level)
                return
        if hasattr(cv2, "setLogLevel"):
            cv2.setLogLevel(0)
    except Exception:
        return


def round_float(value: float, digits: int = 2) -> float:
    return round(float(value), digits)


def backend_name(backend: int) -> str:
    return BACKEND_NAMES.get(int(backend), f"BACKEND_{int(backend)}")


def finite_or_none(value: float | int | None, digits: int = 3) -> float | None:
    if value is None:
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(number):
        return None
    return round_float(number, digits)


def safe_set(cap: cv2.VideoCapture, prop_id: int, value: float) -> bool:
    try:
        return bool(cap.set(prop_id, value))
    except Exception:
        return False


def safe_get(cap: cv2.VideoCapture, prop_id: int) -> float | None:
    try:
        return finite_or_none(cap.get(prop_id))
    except Exception:
        return None


def set_capture_properties(cap: cv2.VideoCapture) -> dict[str, object]:
    applied = {
        "frame_width": safe_set(cap, cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH),
        "frame_height": safe_set(cap, cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT),
        "fps": safe_set(cap, cv2.CAP_PROP_FPS, 30),
        "buffer_size": safe_set(cap, cv2.CAP_PROP_BUFFERSIZE, 1),
        "auto_controls": [],
    }
    for prop_id, value in AUTO_SETTINGS:
        applied["auto_controls"].append(
            {
                "prop_id": int(prop_id),
                "value": value,
                "applied": safe_set(cap, prop_id, value),
            }
        )
    return applied


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
    selection_score = (
        exposure_score * 0.38
        + white_balance_score * 0.20
        + sharpness_score * 0.27
        + stability_score * 0.10
        + contrast_score * 0.05
    )

    highlight_preservation_score = max(0.0, 1.0 - high_clip_pct / 16.0)
    fallback_brightness_score = max(0.0, 1.0 - abs(brightness - 138.0) / 82.0)
    shadow_preservation_score = max(0.0, 1.0 - low_clip_pct / 28.0)
    bright_scene_score = (
        highlight_preservation_score * 0.46
        + fallback_brightness_score * 0.22
        + sharpness_score * 0.17
        + shadow_preservation_score * 0.10
        + white_balance_score * 0.05
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
        "selection_score": round_float(selection_score, 3),
        "highlight_preservation_score": round_float(highlight_preservation_score, 3),
        "shadow_preservation_score": round_float(shadow_preservation_score, 3),
        "bright_scene_score": round_float(bright_scene_score, 3),
    }
    return metrics, gray


def warmup_camera(
    cap: cv2.VideoCapture,
    *,
    min_frames: int = WARMUP_MIN_FRAMES,
    min_seconds: float = WARMUP_MIN_SECONDS,
    max_seconds: float = WARMUP_MAX_SECONDS,
) -> tuple[np.ndarray | None, dict[str, float | int | bool]]:
    start = time.monotonic()
    last_frame = None
    last_gray = None
    stability_history: list[dict[str, float | str]] = []
    warmup_frames = 0
    stabilized = False
    brightness_std = 999.0
    wb_std = 999.0

    while True:
        frame = read_valid_frame(cap, timeout_seconds=0.75)
        elapsed = time.monotonic() - start
        if frame is None:
            if elapsed >= max_seconds:
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

        if warmup_frames >= min_frames and elapsed >= min_seconds and stabilized:
            break
        if elapsed >= max_seconds:
            break

    warmup_summary = {
        "frames": warmup_frames,
        "seconds": round_float(time.monotonic() - start, 3),
        "stabilized": stabilized,
        "brightness_stddev": round_float(brightness_std),
        "white_balance_stddev": round_float(wb_std),
    }
    return last_frame, warmup_summary


def select_best_frame(
    cap: cv2.VideoCapture,
    initial_frame: np.ndarray | None,
    *,
    max_frames: int = SELECTION_FRAME_COUNT,
    max_seconds: float = SELECTION_MAX_SECONDS,
    score_key: str = "selection_score",
) -> tuple[np.ndarray | None, dict[str, object]]:
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

    while len(candidates) < max_frames and (time.monotonic() - start) < max_seconds:
        frame = read_valid_frame(cap, timeout_seconds=0.6)
        if frame is None:
            continue

        metrics, previous_gray = compute_frame_metrics(frame, previous_gray)
        frame_index = len(candidates)
        candidates.append({"frame_index": frame_index, "metrics": metrics})
        if best_metrics is None or float(metrics[score_key]) > float(best_metrics[score_key]):
            best_frame = frame
            best_metrics = metrics

    selection_summary = {
        "candidate_count": len(candidates),
        "score_key": score_key,
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


def compact_selection_summary(selection_summary: dict[str, object], *, include_candidates: bool) -> dict[str, object]:
    compact = {
        "candidate_count": selection_summary["candidate_count"],
        "score_key": selection_summary["score_key"],
        "selected_frame_index": selection_summary["selected_frame_index"],
        "selection_seconds": selection_summary["selection_seconds"],
        "best_metrics": selection_summary["best_metrics"],
    }
    if include_candidates:
        compact["candidates"] = [
            {
                "frame_index": candidate["frame_index"],
                "mean_brightness": candidate["mean_brightness"],
                "low_clip_pct": candidate["low_clip_pct"],
                "high_clip_pct": candidate["high_clip_pct"],
                "sharpness_laplacian_var": candidate["sharpness_laplacian_var"],
                "white_balance_delta_pct": candidate["white_balance_delta_pct"],
                "selection_score": candidate["selection_score"],
                "bright_scene_score": candidate["bright_scene_score"],
            }
            for candidate in selection_summary["candidates"]
        ]
    return compact


def is_bright_scene(metrics: dict[str, float | str]) -> bool:
    return (
        float(metrics["mean_brightness"]) >= BRIGHT_SCENE_TRIGGER_BRIGHTNESS
        or float(metrics["high_clip_pct"]) >= BRIGHT_SCENE_TRIGGER_HIGHLIGHT_CLIP
    )


def build_exposure_candidates(current_exposure: float | None) -> list[float]:
    candidates: list[float] = []
    if current_exposure is not None:
        candidates.append(current_exposure)
        if current_exposure <= 0.0:
            for delta in (1.0, 2.0, 3.0, 4.0):
                candidates.append(current_exposure - delta)
        elif current_exposure < 1.0:
            for factor in (0.5, 0.25, 0.125):
                candidates.append(current_exposure * factor)
        else:
            for factor in (0.5, 0.25, 0.125):
                candidates.append(current_exposure * factor)

    candidates.extend(COMMON_NEGATIVE_EXPOSURES)
    candidates.extend(COMMON_POSITIVE_EXPOSURES)

    deduped: list[float] = []
    seen: set[float] = set()
    for candidate in candidates:
        rounded = round(candidate, 6)
        if rounded in seen:
            continue
        seen.add(rounded)
        deduped.append(candidate)
        if len(deduped) >= MAX_EXPOSURE_CANDIDATES:
            break
    return deduped


def is_fallback_improvement(
    baseline_metrics: dict[str, float | str],
    candidate_metrics: dict[str, float | str],
) -> bool:
    baseline_high_clip = float(baseline_metrics["high_clip_pct"])
    candidate_high_clip = float(candidate_metrics["high_clip_pct"])
    high_clip_reduction = baseline_high_clip - candidate_high_clip
    brightness_ok = float(candidate_metrics["mean_brightness"]) >= BRIGHT_SCENE_MIN_BRIGHTNESS
    low_clip_ok = float(candidate_metrics["low_clip_pct"]) <= BRIGHT_SCENE_MAX_LOW_CLIP
    sharpness_ok = float(candidate_metrics["sharpness_laplacian_var"]) >= max(
        BRIGHT_SCENE_MIN_SHARPNESS,
        float(baseline_metrics["sharpness_laplacian_var"]) * 0.45,
    )

    if not (brightness_ok and low_clip_ok and sharpness_ok):
        return False

    if candidate_high_clip <= BRIGHT_SCENE_GOAL_HIGHLIGHT_CLIP and high_clip_reduction >= 2.0:
        return True
    if high_clip_reduction >= 6.0:
        return True
    if high_clip_reduction >= 3.0 and float(candidate_metrics["bright_scene_score"]) > float(baseline_metrics["bright_scene_score"]) + 0.06:
        return True
    return False


def attempt_bright_scene_fallback(
    cap: cv2.VideoCapture,
    *,
    backend: int,
    baseline_metrics: dict[str, float | str],
) -> tuple[np.ndarray | None, dict[str, object]]:
    current_exposure = safe_get(cap, cv2.CAP_PROP_EXPOSURE)
    attempts: list[dict[str, object]] = []
    best_frame = None
    best_summary = None
    best_metrics = None
    exposure_candidates = build_exposure_candidates(current_exposure)

    for manual_mode in MANUAL_AUTO_EXPOSURE_VALUES:
        for exposure_value in exposure_candidates:
            auto_mode_applied = safe_set(cap, cv2.CAP_PROP_AUTO_EXPOSURE, manual_mode)
            exposure_applied = safe_set(cap, cv2.CAP_PROP_EXPOSURE, exposure_value)
            safe_set(cap, cv2.CAP_PROP_AUTO_WB, 1.0)
            safe_set(cap, cv2.CAP_PROP_AUTOFOCUS, 1.0)
            time.sleep(FALLBACK_SETTLE_SECONDS)

            warmup_frame, warmup_summary = warmup_camera(
                cap,
                min_frames=FALLBACK_WARMUP_MIN_FRAMES,
                min_seconds=FALLBACK_WARMUP_MIN_SECONDS,
                max_seconds=FALLBACK_WARMUP_MAX_SECONDS,
            )
            frame, selection_summary = select_best_frame(
                cap,
                warmup_frame,
                max_frames=FALLBACK_SELECTION_FRAME_COUNT,
                max_seconds=FALLBACK_SELECTION_MAX_SECONDS,
                score_key="bright_scene_score",
            )

            if frame is None or selection_summary["best_metrics"] is None:
                attempts.append(
                    {
                        "manual_auto_exposure": manual_mode,
                        "requested_exposure": exposure_value,
                        "auto_mode_applied": auto_mode_applied,
                        "exposure_applied": exposure_applied,
                        "reported_exposure": safe_get(cap, cv2.CAP_PROP_EXPOSURE),
                        "reported_auto_exposure": safe_get(cap, cv2.CAP_PROP_AUTO_EXPOSURE),
                        "result": "no_valid_frame",
                    }
                )
                continue

            compact_selection = compact_selection_summary(selection_summary, include_candidates=True)
            metrics = compact_selection["best_metrics"]
            attempt_summary = {
                "backend_name": backend_name(backend),
                "manual_auto_exposure": manual_mode,
                "requested_exposure": exposure_value,
                "auto_mode_applied": auto_mode_applied,
                "exposure_applied": exposure_applied,
                "reported_exposure": safe_get(cap, cv2.CAP_PROP_EXPOSURE),
                "reported_auto_exposure": safe_get(cap, cv2.CAP_PROP_AUTO_EXPOSURE),
                "warmup": warmup_summary,
                "selection": compact_selection,
                "high_clip_reduction_pct": round_float(
                    float(baseline_metrics["high_clip_pct"]) - float(metrics["high_clip_pct"]),
                    2,
                ),
                "result": "captured",
            }
            attempts.append(attempt_summary)

            if best_metrics is None or float(metrics["bright_scene_score"]) > float(best_metrics["bright_scene_score"]):
                best_frame = frame
                best_metrics = metrics
                best_summary = attempt_summary

    used = False
    reason = "manual_exposure_attempted_no_improvement"
    if best_metrics is None:
        reason = "manual_exposure_attempted_no_valid_frame"
    elif is_fallback_improvement(baseline_metrics, best_metrics):
        used = True
        reason = "manual_exposure_selected"
    elif float(best_metrics["high_clip_pct"]) >= float(baseline_metrics["high_clip_pct"]):
        reason = "manual_exposure_attempted_no_clip_reduction"
    else:
        reason = "manual_exposure_attempted_reduction_not_safe"

    summary = {
        "triggered": True,
        "used": used,
        "strategy": "manual_exposure_bracketing",
        "reason": reason,
        "baseline_metrics": baseline_metrics,
        "baseline_exposure": current_exposure,
        "attempt_count": len(attempts),
        "best_attempt": best_summary,
        "best_metrics": best_metrics,
        "attempts": attempts,
    }
    return (best_frame if used else None), summary


def try_camera(index: int) -> tuple[np.ndarray | None, dict[str, object] | None]:
    errors: list[str] = []
    for backend in BACKENDS:
        cap = cv2.VideoCapture(index, backend)
        if not cap.isOpened():
            cap.release()
            errors.append(f"{backend_name(backend)} open_failed")
            continue

        try:
            property_summary = set_capture_properties(cap)
            warmup_frame, warmup_summary = warmup_camera(cap)
            auto_frame, auto_selection = select_best_frame(cap, warmup_frame)
            if auto_frame is None or auto_selection["best_metrics"] is None:
                errors.append(f"{backend_name(backend)} no_valid_frame")
                continue

            auto_selection = compact_selection_summary(auto_selection, include_candidates=True)
            selected_frame = auto_frame
            selected_selection = auto_selection
            adaptive_fallback = {
                "triggered": False,
                "used": False,
                "strategy": "manual_exposure_bracketing",
                "reason": "not_needed",
            }

            baseline_metrics = auto_selection["best_metrics"]
            if is_bright_scene(baseline_metrics):
                fallback_frame, adaptive_fallback = attempt_bright_scene_fallback(
                    cap,
                    backend=backend,
                    baseline_metrics=baseline_metrics,
                )
                if fallback_frame is not None and adaptive_fallback["best_attempt"] is not None:
                    selected_frame = fallback_frame
                    selected_selection = adaptive_fallback["best_attempt"]["selection"]

            return selected_frame, {
                "camera_index": index,
                "backend": int(backend),
                "backend_name": backend_name(backend),
                "capture_properties": property_summary,
                "warmup": warmup_summary,
                "selection": selected_selection,
                "auto_selection": auto_selection,
                "adaptive_fallback": adaptive_fallback,
            }
        finally:
            cap.release()

    return None, {"camera_index": index, "errors": errors}


def build_notes(metrics: dict[str, float | str], adaptive_fallback: dict[str, object]) -> list[str]:
    notes: list[str] = []
    if float(metrics["mean_brightness"]) < 90:
        notes.append("Image is dark; GPT should account for underexposure.")
    elif float(metrics["mean_brightness"]) > 175:
        notes.append("Image is bright; GPT should watch for highlight washout.")

    if metrics["white_balance_cast"] != "neutral":
        notes.append(f"White balance trends {metrics['white_balance_cast']}.")

    if float(metrics["sharpness_laplacian_var"]) < 80:
        notes.append("Frame sharpness is low; fine text may be unreliable.")

    if float(metrics["high_clip_pct"]) > 2.5 or float(metrics["low_clip_pct"]) > 10.0:
        notes.append("Clipping is present in shadows or highlights.")

    if float(metrics["motion_mean_abs_diff"]) > 8.0:
        notes.append("Scene motion was detected during frame selection.")

    if adaptive_fallback.get("triggered"):
        if adaptive_fallback.get("used"):
            notes.append("Bright-scene fallback selected a lower-exposure frame to preserve highlights.")
        else:
            notes.append(
                "Bright-scene fallback ran but did not find a safer frame that improved highlights without hurting readability."
            )

    if not notes:
        notes.append("Exposure, white balance, and sharpness are within the expected range.")
    return notes


def write_metadata(metadata: dict[str, object]) -> None:
    METADATA_PATH.write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def main() -> None:
    configure_opencv_logging()
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
        adaptive_fallback = capture["adaptive_fallback"]
        notes = build_notes(image_metrics, adaptive_fallback)
        captured_at = datetime.now().astimezone().isoformat(timespec="seconds")

        top_level_fallback = {
            "triggered": bool(adaptive_fallback.get("triggered", False)),
            "used": bool(adaptive_fallback.get("used", False)),
            "strategy": adaptive_fallback.get("strategy"),
            "reason": adaptive_fallback.get("reason"),
        }
        if adaptive_fallback.get("best_metrics") is not None:
            top_level_fallback["best_metrics"] = adaptive_fallback["best_metrics"]
        if adaptive_fallback.get("best_attempt") is not None:
            top_level_fallback["best_attempt"] = {
                "manual_auto_exposure": adaptive_fallback["best_attempt"]["manual_auto_exposure"],
                "requested_exposure": adaptive_fallback["best_attempt"]["requested_exposure"],
                "reported_exposure": adaptive_fallback["best_attempt"]["reported_exposure"],
                "high_clip_reduction_pct": adaptive_fallback["best_attempt"]["high_clip_reduction_pct"],
            }

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
                "score_key": capture["selection"]["score_key"],
                "best_metrics": capture["selection"]["best_metrics"],
            },
            "image_metrics": image_metrics,
            "adaptive_fallback": top_level_fallback,
            "notes": notes,
            "gpt_handoff": {
                "artifact_type": "image_with_metadata",
                "image_path": str(OUTPUT_PATH),
                "metadata_path": str(METADATA_PATH),
                "prompt": (
                    "Analyze the attached USB camera snapshot of the ESP32 sensor hub test setup. "
                    "Use the metadata JSON for capture quality signals, especially image_metrics and adaptive_fallback. "
                    "Check framing, glare, blur, display readability, device presence, cable visibility, and any obvious test setup issues."
                ),
                "checklist": [
                    "Is the device or dashboard visible and reasonably framed?",
                    "Is the image too dark, too bright, blurry, or color shifted?",
                    "Did the bright-scene fallback reduce highlight loss, or does the metadata still report clipping risk?",
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
                "capture_properties": capture["capture_properties"],
                "selection": capture["selection"],
                "auto_selection": capture["auto_selection"],
                "adaptive_fallback": adaptive_fallback,
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
