#include "imageanalysis.h"

#include <cjson\cJSON.h>
#include <stdio.h>


double NormalizeValue(double fValue, double fOrigRange, double fMinOrig, double fNewRange, double fMinNew)
{
    return fOrigRange ? ((fValue - fMinOrig) / fOrigRange) * fNewRange + fMinNew : fMinNew;
}

void UpdatePrintAnalysisOpts(ImageAnalysis* pImageAnalysis, AnalysisOpts* pOpts)
{
    if (pOpts)
    {
        pImageAnalysis->opts = *pOpts;
    }
}

gboolean ParsePartitionsFromString(ImageAnalysis* pImageAnalysis, const gchar* pJsonStr)
{
    cJSON* pJson = cJSON_Parse(pJsonStr);
    if (pJson == NULL)
    {
        return FALSE;
    }

    // Get "partitions" array
    cJSON* pPartitions = cJSON_GetObjectItem(pJson, "partitions");
    if (!cJSON_IsArray(pPartitions))
    {
        cJSON_Delete(pJson);
        return FALSE;
    }

    if (pImageAnalysis->pPartitions)
        free(pImageAnalysis->pPartitions);

    // Iterate through the array
    int nPartitions = cJSON_GetArraySize(pPartitions);
    
    pImageAnalysis->pPartitions = calloc(nPartitions, sizeof(PrintPartition));
    pImageAnalysis->nPartitions = 0;

    for (int i = 0; i < nPartitions; i++)
    {
        cJSON* partition = cJSON_GetArrayItem(pPartitions, i);

        // Extract values from each partition
        cJSON* id = cJSON_GetObjectItem(partition, "id");
        cJSON* center_x = cJSON_GetObjectItem(partition, "center_x");
        cJSON* center_y = cJSON_GetObjectItem(partition, "center_y");
        cJSON* width = cJSON_GetObjectItem(partition, "width");
        cJSON* height = cJSON_GetObjectItem(partition, "height");
        cJSON* bg_r = cJSON_GetObjectItem(partition, "bg_r");
        cJSON* bg_g = cJSON_GetObjectItem(partition, "bg_g");
        cJSON* bg_b = cJSON_GetObjectItem(partition, "bg_b");

        if (cJSON_IsNumber(id) && cJSON_IsNumber(center_x) && cJSON_IsNumber(center_y) &&
            cJSON_IsNumber(width) && cJSON_IsNumber(height)&& cJSON_IsNumber(bg_r) &&
            cJSON_IsNumber(bg_g) && cJSON_IsNumber(bg_b))
        {

            pImageAnalysis->pPartitions[i].id = id->valueint;
            pImageAnalysis->pPartitions[i].centerX = center_x->valueint;
            pImageAnalysis->pPartitions[i].centerY = center_y->valueint;
            pImageAnalysis->pPartitions[i].width = width->valueint;
            pImageAnalysis->pPartitions[i].height = height->valueint;
			pImageAnalysis->pPartitions[i].bg.rgb.r = bg_r->valueint;
			pImageAnalysis->pPartitions[i].bg.rgb.g = bg_g->valueint;
			pImageAnalysis->pPartitions[i].bg.rgb.b = bg_b->valueint;
            pImageAnalysis->nPartitions++;
        }
    }

    // Clean up
    cJSON_Delete(pJson);
    return TRUE;
}

char* PartitionsArrayToJsonStr(ImageAnalysis* pImageAnalysis)
{
    char* pJsonStr = NULL;
    cJSON* root = cJSON_CreateObject();
    
    if (!root)
        goto cleanup;

    if (!pImageAnalysis->nPartitions)
        goto cleanup;

    // Create an array for partitions
    cJSON* pPartitions = cJSON_CreateArray();
    if (!pPartitions)
        goto cleanup;

    // Add partitions array to the root object
    cJSON_AddItemToObject(root, "partitions", pPartitions);

    // Iterate through the array and add each partition to the JSON array
    for (int i = 0; i < pImageAnalysis->nPartitions; ++i)
    {
        cJSON* pPartition = cJSON_CreateObject();
        if (!pPartition)
            continue;

        // Add fields to the partition object
        cJSON_AddNumberToObject(pPartition, "id", pImageAnalysis->pPartitions[i].id);
        //cJSON_AddNumberToObject(pPartition, "center_x", pImageAnalysis->pPartitions[i].centerX);
        //cJSON_AddNumberToObject(pPartition, "center_y", pImageAnalysis->pPartitions[i].centerY);
        //cJSON_AddNumberToObject(pPartition, "width", pImageAnalysis->pPartitions[i].width);
        //cJSON_AddNumberToObject(pPartition, "height", pImageAnalysis->pPartitions[i].height);

        // Add RGB or YUV based on your data
        cJSON* total = cJSON_CreateObject();
        if (!total)
        {
            cJSON_Delete(pPartition);
            continue;
        }

        char pTmpStr[128];
        Pixel* pTmp = NULL;
        
        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d", pImageAnalysis->pPartitions[i].total.rgb.r, pImageAnalysis->pPartitions[i].total.rgb.g, pImageAnalysis->pPartitions[i].total.rgb.b);
        cJSON_AddStringToObject(pPartition, "total", pTmpStr);

        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d", pImageAnalysis->pPartitions[i].avg.rgb.r, pImageAnalysis->pPartitions[i].avg.rgb.g, pImageAnalysis->pPartitions[i].avg.rgb.b);
        cJSON_AddStringToObject(pPartition, "average", pTmpStr);

        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d", pImageAnalysis->pPartitions[i].min.rgb.r, pImageAnalysis->pPartitions[i].min.rgb.g, pImageAnalysis->pPartitions[i].min.rgb.b);
        cJSON_AddStringToObject(pPartition, "min", pTmpStr);

        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d", pImageAnalysis->pPartitions[i].max.rgb.r, pImageAnalysis->pPartitions[i].max.rgb.g, pImageAnalysis->pPartitions[i].max.rgb.b);
        cJSON_AddStringToObject(pPartition, "max", pTmpStr);

        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d", pImageAnalysis->pPartitions[i].nonUniformity.rgb.r, pImageAnalysis->pPartitions[i].nonUniformity.rgb.g, pImageAnalysis->pPartitions[i].nonUniformity.rgb.b);
        cJSON_AddStringToObject(pPartition, "non-uniformity", pTmpStr);

        pTmp = &pImageAnalysis->pPartitions[i].minSat;
        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d,%d", pTmp->rgb.r,pTmp->rgb.g,pTmp->rgb.b,pTmp->rgb.k);
        cJSON_AddStringToObject(pPartition, "saturation_min", pTmpStr);
 
        pTmp = &pImageAnalysis->pPartitions[i].maxSat;
        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d,%d", pTmp->rgb.r, pTmp->rgb.g, pTmp->rgb.b, pTmp->rgb.k);
        cJSON_AddStringToObject(pPartition, "saturation_max", pTmpStr);
 
        pTmp = &pImageAnalysis->pPartitions[i].avgSat;
        snprintf(pTmpStr, sizeof(pTmpStr), "%d,%d,%d,%d", pTmp->rgb.r, pTmp->rgb.g, pTmp->rgb.b, pTmp->rgb.k);
        cJSON_AddStringToObject(pPartition, "saturation_avg", pTmpStr);

        // Add the partition to the array
        cJSON_AddItemToArray(pPartitions, pPartition);
    }

    // Convert the root JSON object to a string
    pJsonStr = cJSON_Print(root);

cleanup:
    cJSON_Delete(root);
    return pJsonStr;
}
