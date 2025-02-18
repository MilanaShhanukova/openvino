# Copyright (C) 2018-2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import re
from argparse import Namespace

from openvino.tools.mo.utils.error import Error
from openvino.tools.mo.utils.utils import refer_to_faq_msg


def deduce_framework_by_namespace(argv: Namespace):
    if not argv.framework:
        if getattr(argv, 'saved_model_dir', None) or getattr(argv, 'input_meta_graph', None):
            argv.framework = 'tf'
        elif getattr(argv, 'input_symbol', None) or getattr(argv, 'pretrained_model_name', None):
            argv.framework = 'mxnet'
        elif getattr(argv, 'input_proto', None):
            argv.framework = 'caffe'
        elif argv.input_model is None:
            raise Error('Path to input model is required: use --input_model.')
        else:
            argv.framework = guess_framework_by_ext(argv.input_model)
        if not argv.framework:
            raise Error('Framework name can not be deduced from the given options: {}={}. Use --framework to choose '
                        'one of caffe, tf, mxnet, kaldi, onnx', '--input_model', argv.input_model, refer_to_faq_msg(15))

    return map(lambda x: argv.framework == x, ['tf', 'caffe', 'mxnet', 'kaldi', 'onnx'])


def guess_framework_by_ext(input_model_path: str) -> int:
    if re.match(r'^.*\.caffemodel$', input_model_path):
        return 'caffe'
    elif re.match(r'^.*\.pb$', input_model_path):
        return 'tf'
    elif re.match(r'^.*\.pbtxt$', input_model_path):
        return 'tf'
    elif re.match(r'^.*\.params$', input_model_path):
        return 'mxnet'
    elif re.match(r'^.*\.nnet$', input_model_path):
        return 'kaldi'
    elif re.match(r'^.*\.mdl', input_model_path):
        return 'kaldi'
    elif re.match(r'^.*\.onnx$', input_model_path):
        return 'onnx'

