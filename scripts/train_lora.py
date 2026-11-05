#!/usr/bin/env python3
"""Offline supervised LoRA fine-tuning of successful Argxx action trajectories."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    checkpoint = Path(config['base_model']).expanduser()
    dataset = Path(config['dataset'])
    if not checkpoint.is_dir() or not (checkpoint / 'config.json').is_file():
        raise SystemExit('Set base_model to a local Hugging Face checkpoint; Ollama GGUF weights are not a training checkpoint.')
    if not dataset.is_file():
        raise SystemExit('Training JSONL is missing. Export successful model runs or supply reviewed demonstrations.')
    try:
        import torch
        from peft import LoraConfig, get_peft_model
        from transformers import AutoModelForCausalLM, AutoTokenizer, Trainer, TrainingArguments, set_seed
    except ImportError as error:
        raise SystemExit('Install scripts/requirements-training.txt in a virtual environment: ' + str(error))
    output = Path(config['output_dir'])
    if output.exists() and any(output.iterdir()):
        raise SystemExit('Choose an empty output_dir to preserve previous adapters.')
    set_seed(config['seed'])
    tokenizer = AutoTokenizer.from_pretrained(str(checkpoint), local_files_only=True)
    if tokenizer.pad_token_id is None:
        tokenizer.pad_token = tokenizer.eos_token
    examples = []
    skipped = 0
    for line in dataset.read_text().splitlines():
        if not line.strip():
            continue
        messages = json.loads(line)['messages']
        if not messages or messages[-1]['role'] != 'assistant':
            raise ValueError('Each example must end with an assistant action')
        prefix = tokenizer.apply_chat_template(messages[:-1], tokenize=True, add_generation_prompt=True)
        tokens = tokenizer.apply_chat_template(messages, tokenize=True, add_generation_prompt=False)
        if tokens[:len(prefix)] != prefix:
            raise ValueError('Chat template prefix is not stable; cannot safely mask prompt tokens')
        if len(tokens) > config['max_length']:
            skipped += 1
            continue
        labels = [-100] * len(prefix) + tokens[len(prefix):]
        if all(x == -100 for x in labels):
            raise ValueError('Example has no assistant tokens')
        examples.append({'input_ids': tokens, 'attention_mask': [1] * len(tokens), 'labels': labels})
    if not examples:
        raise SystemExit('No usable examples; check max_length and chat template.')

    def collate(rows):
        width = max(len(row['input_ids']) for row in rows)
        return {key: torch.tensor([row[key] + [pad] * (width - len(row[key])) for row in rows])
                for key, pad in [('input_ids', tokenizer.pad_token_id), ('attention_mask', 0), ('labels', -100)]}

    started = time.monotonic()
    model = AutoModelForCausalLM.from_pretrained(str(checkpoint), local_files_only=True)
    model.config.use_cache = False
    model = get_peft_model(model, LoraConfig(task_type='CAUSAL_LM', r=config['rank'],
        lora_alpha=config['alpha'], target_modules=config['target_modules'], lora_dropout=0.05))
    training_args = TrainingArguments(output_dir=str(output / 'checkpoints'),
        num_train_epochs=config['epochs'], learning_rate=config['learning_rate'],
        per_device_train_batch_size=1, gradient_accumulation_steps=config['gradient_accumulation_steps'],
        logging_steps=1, save_strategy='no', report_to='none', seed=config['seed'])
    trainer = Trainer(model=model, args=training_args, train_dataset=examples,
                      data_collator=collate, processing_class=tokenizer)
    result = trainer.train()
    model.save_pretrained(output, safe_serialization=True)
    tokenizer.save_pretrained(output / 'tokenizer')
    report = {'config': config, 'examples': len(examples), 'skipped_long': skipped,
              'seconds': time.monotonic() - started, 'metrics': result.metrics,
              'dataset_sha256': hashlib.sha256(dataset.read_bytes()).hexdigest(),
              'versions': {p: importlib.metadata.version(p) for p in ['torch', 'transformers', 'peft', 'accelerate']}}
    (output / 'training_report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Saved trained adapter to {output}. Evaluate it separately before claiming improvement.')


if __name__ == '__main__':
    main()
