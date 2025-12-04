#!/usr/bin/env python3
"""
Скрипт для конвертации аудиофайлов в формат, необходимый для обучения модели wake word.

Требования:
- Формат: WAV
- Частота дискретизации: 16kHz
- Каналы: моно
- Битность: 16-bit PCM
"""

import os
import sys
import argparse
from pathlib import Path

try:
    import soundfile as sf
    import librosa
except ImportError:
    print("Ошибка: Необходимо установить библиотеки:")
    print("  pip install soundfile librosa")
    sys.exit(1)


def convert_audio(input_file, output_file, target_sr=16000, target_channels=1):
    """
    Конвертирует аудиофайл в нужный формат.
    
    Args:
        input_file: Путь к входному файлу
        output_file: Путь к выходному файлу
        target_sr: Целевая частота дискретизации (по умолчанию 16kHz)
        target_channels: Целевое количество каналов (1 = моно)
    """
    try:
        # Загружаем аудио
        audio, sr = librosa.load(input_file, sr=None, mono=False)
        
        # Конвертируем в моно, если нужно
        if len(audio.shape) > 1:
            audio = librosa.to_mono(audio)
        
        # Ресемплируем, если нужно
        if sr != target_sr:
            audio = librosa.resample(audio, orig_sr=sr, target_sr=target_sr)
        
        # Сохраняем в формате WAV, 16-bit PCM
        sf.write(output_file, audio, target_sr, subtype='PCM_16', format='WAV')
        
        print(f"✓ Конвертирован: {input_file} -> {output_file}")
        return True
        
    except Exception as e:
        print(f"✗ Ошибка при конвертации {input_file}: {e}")
        return False


def process_directory(input_dir, output_dir, recursive=False):
    """
    Обрабатывает все аудиофайлы в директории.
    
    Args:
        input_dir: Входная директория
        output_dir: Выходная директория
        recursive: Обрабатывать поддиректории рекурсивно
    """
    input_path = Path(input_dir)
    output_path = Path(output_dir)
    
    if not input_path.exists():
        print(f"Ошибка: Директория {input_dir} не существует")
        return False
    
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Поддерживаемые форматы
    audio_extensions = {'.wav', '.mp3', '.flac', '.m4a', '.ogg', '.aac'}
    
    # Находим все аудиофайлы
    if recursive:
        audio_files = []
        for ext in audio_extensions:
            audio_files.extend(input_path.rglob(f'*{ext}'))
    else:
        audio_files = []
        for ext in audio_extensions:
            audio_files.extend(input_path.glob(f'*{ext}'))
    
    if not audio_files:
        print(f"Предупреждение: Не найдено аудиофайлов в {input_dir}")
        return False
    
    print(f"Найдено {len(audio_files)} аудиофайлов")
    
    success_count = 0
    for audio_file in audio_files:
        # Сохраняем структуру директорий, если recursive
        if recursive:
            relative_path = audio_file.relative_to(input_path)
            output_file = output_path / relative_path.with_suffix('.wav')
            output_file.parent.mkdir(parents=True, exist_ok=True)
        else:
            output_file = output_path / audio_file.with_suffix('.wav').name
        
        if convert_audio(str(audio_file), str(output_file)):
            success_count += 1
    
    print(f"\nУспешно конвертировано: {success_count}/{len(audio_files)} файлов")
    return success_count > 0


def main():
    parser = argparse.ArgumentParser(
        description='Конвертация аудиофайлов для обучения модели wake word'
    )
    parser.add_argument(
        'input',
        help='Входной файл или директория'
    )
    parser.add_argument(
        'output',
        help='Выходной файл или директория'
    )
    parser.add_argument(
        '-r', '--recursive',
        action='store_true',
        help='Рекурсивная обработка поддиректорий'
    )
    parser.add_argument(
        '--sample-rate',
        type=int,
        default=16000,
        help='Целевая частота дискретизации (по умолчанию: 16000)'
    )
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_path = Path(args.output)
    
    if input_path.is_file():
        # Обработка одного файла
        output_path.parent.mkdir(parents=True, exist_ok=True)
        convert_audio(str(input_path), str(output_path), args.sample_rate)
    elif input_path.is_dir():
        # Обработка директории
        process_directory(str(input_path), str(output_path), args.recursive)
    else:
        print(f"Ошибка: {args.input} не является файлом или директорией")
        sys.exit(1)


if __name__ == '__main__':
    main()

