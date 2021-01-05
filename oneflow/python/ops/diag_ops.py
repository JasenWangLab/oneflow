"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
from __future__ import absolute_import

import oneflow as flow
import oneflow.python.framework.id_util as id_util
from oneflow.python.oneflow_export import oneflow_export
import oneflow.python.framework.remote_blob as remote_blob_util
from typing import Optional


@oneflow_export("diag")
def tensor_list_to_tensor_buffer(
    input_tensor: remote_blob_util.BlobDef, 
    dimension: Optional[int] = 0,
    name: Optional[str] = None
) -> remote_blob_util.BlobDef:
    """This operator compute diagonal. 

    Refer to `Concept Explanation <https://docs.oneflow.org/basics_topics/concept_explanation.html#3tensorbuffer-tensorlist>`_ 
    for more about Diag. 

    Args:
        input (remote_blob_util.BlobDef): The input `input_tensor`. 
        name (Optional[str], optional): The name for the operation. Defaults to None.

    Returns:
        remote_blob_util.BlobDef: The result Blob. 

    For example: 

    .. code-block:: python 

        import oneflow as flow
        import numpy as np
        import oneflow.typing as tp

        func_config = flow.FunctionConfig()
        func_config.default_data_type(flow.float)
        func_config.default_logical_view(flow.scope.mirrored_view())
        @flow.global_function(function_config=func_config)
        def diag_Job(x: tp.ListListNumpy.Placeholder(shape=(2, 5), dtype=flow.float32),
        ) -> tp.ListListNumpy:
            x = flow.tensor_list_to_tensor_buffer(input=x)
            return flow.diag(x, 
                            dtype=flow.float32)

        x = np.random.rand(1, 3, 2).astype(np.float32)
        y = np.random.rand(1, 2, 2).astype(np.float32)
        out = tensorList_to_tensorBuffer_Job([[x, y]])

        # out[0][0].shape (1, 3, 2)

    """
    return (
        flow.user_op_builder(
            name if name is not None else id_util.UniqueStr("Diag_")
        )
        .Op("diag")
        .Input("input_tensor", [input_tensor])
        .Attr("dimension", int(dimension))
        .Output("diag_out")
        .Build() 
        .InferAndTryRun()
        .RemoteBlobList()[0]
    )
    
    

