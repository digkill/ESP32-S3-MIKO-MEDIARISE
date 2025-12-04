# Скрипты для обучения кастомной модели wake word

## Описание

Эта директория содержит вспомогательные скрипты для подготовки данных и обучения кастомной модели wake word.

## Скрипты

### convert_audio.py

Конвертирует аудиофайлы в формат, необходимый для обучения (WAV, 16kHz, моно, 16-bit PCM).

**Использование:**

```bash
# Конвертировать один файл
python convert_audio.py input.mp3 output.wav

# Конвертировать все файлы в директории
python convert_audio.py input_dir/ output_dir/

# Рекурсивная обработка
python convert_audio.py -r input_dir/ output_dir/
```

**Требования:**
```bash
pip install -r requirements.txt
```

Или установить вручную:
```bash
pip install soundfile librosa numpy
```

## Структура данных

После подготовки данных структура должна выглядеть так:

```
training_data/
├── wake_word/          # Записи вашего wake word
│   ├── sample1.wav
│   ├── sample2.wav
│   └── ...
├── background/         # Фоновый шум
│   ├── noise1.wav
│   └── ...
└── negative/          # Другие слова (не wake word)
    ├── other1.wav
    └── ...
```

## Следующие шаги

После подготовки данных следуйте инструкциям в `docs/custom-wake-word-training.md` для обучения модели.

